/*
一、路径的实现
    1. 所有的文件夹和文件都需要有唯一的路径标识key
    2. 没有必要 分文件/文件夹两个vector存储。
    3. 文件夹应提供 父文件夹、子文件夹 和 子文件 的路径标识
    4. 文件应提供自身的信息，比如name、type和content
    5. 需要实现删除和插入功能
二、文本读写
    1. 其实没那么难🤪但是撤销write就难了🤡
三、撤销与重做
    1. 总原则：
        每次成功的操作都要传递一些参数，然后带上执行的命令存入 指令栈；
        每次undo利用指令栈的信息撤回，同时再带上一些信息存入  撤销栈；
        每次redo利用撤销栈的信息恢复，同时再带上一些信息存入  指令栈。
    2. 需要的信息(我认为应该已经足够🤔)
        1. mkdir    fid; dir_name; dir_id
        2. create   fid; file_name; file_id
        3. append   file_id; content
        4. cd       pre_id; post_id
        5. write    file_id; offset; newhexdata; prehexdata; prelen
                    // 记录prelen是因为当new写在pre后面，需要知道从哪开始截断0
                    // 事实证明还有一个好处：不用多写一次ed
        6. rm       fid; variant_name; variant_id
*/

#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <variant>
#include <algorithm>

using namespace std;

enum Type {txt,md,bin};
struct dir{
    int fid; // father dir's id
    map<string, int> child; // children 's name & id.sorted.
    // remember that file and dir cannot share the same name
};
struct file{
    Type type;
    string text;// for the txt & md
    vector<unsigned char> content;// for the bin
};
vector<variant<dir,file>> datas; 
int cur_dir_id = 0;

struct do_make{
    int fid, variant_id;
    string variant_name;
};
struct do_append{
    int file_id;
    string content;
};
struct do_cd{
    int pre_id, post_id;
};
struct do_write{
    int file_id, offset, prelen;
    vector<unsigned char>newhexdata,prehexdata;
};
struct do_rm{
    int fid, variant_id;
    string variant_name;
};
vector<variant<do_make,do_append,do_cd,do_write,do_rm>>undo_stack;

//command
void mkdir(){
    string name;
    cin >> name;
    if (get<dir>(datas[cur_dir_id]).child.count(name) > 0) { 
        cout << "Invalid operation\n";
        return;
    }
    datas.push_back((dir){cur_dir_id,{}});
    dir &curd = get<dir>(datas[cur_dir_id]); // cur_dir
    curd.child.insert({name , datas.size() - 1});
    undo_stack.push_back(do_make{cur_dir_id,(int)datas.size() - 1, name});
}
void cd() {
    // 由于操作失败视为不执行，因而不能立刻修改cur_dir_id !
    int try_id = cur_dir_id;
    string path;
    cin >> path;
    int pos = 0;
    if (path[0] == '/') { 
        try_id = 0;
        pos = 1;
    }
    string nowdir = "";
    while (pos < path.size() || nowdir != "") { // to check
        if (pos == path.size() || path[pos] == '/') {
            if (nowdir == "..") {
                if (try_id == 0) {
                    cout << "Invalid operation\n";
                    return;
                }
                else {
                    try_id = get<dir>(datas[try_id]).fid;
                }
            }
            else if (nowdir == ".") {}
            else if (get<dir>(datas[try_id]).child.count(nowdir) == 0 ){ // not existed
                cout << "Invalid operation\n";
                return;
            }
            else {
                int new_id = get<dir>(datas[try_id]).child[nowdir];
                if (holds_alternative<file>(datas[new_id])) {// a file not a dir
                    cout << "Invalid operation\n";
                    return;
                }
                try_id = new_id;
            }
            nowdir = "";
        }
        else nowdir += path[pos];
        pos++;
    }
    undo_stack.push_back(do_cd{cur_dir_id,try_id});
    cur_dir_id = try_id;
}
void newcontent (string &content) {
    int pos = 0;
    while (content[pos] != '\"') pos++;
    content = content.substr(pos + 1,content.size() - pos - 1);
    pos = content.size() - 1;
    while (content[pos] != '\"') pos--;
    content.resize(pos);
}
vector<unsigned char>split(const string &line){
    vector<unsigned char>tok;
    string word="";
    for (char ch : line){
        if (ch==' '){
            if (!word.empty()){
                int x = stoi(word, nullptr, 16);
                tok.push_back(static_cast<unsigned char>(x)); // don't forget the ()
                word = "";
            }
        }
        else{
            word+=ch;
        }
    }
    if (!word.empty()){
        int x = stoi(word, nullptr, 16);
        tok.push_back(static_cast<unsigned char>(x)); // don't forget the ()
    }
    return tok;
}
void create(){
    string type,name,content;
    cin >> type >> name;
    getline(cin , content);
    newcontent(content);
    if (get<dir>(datas[cur_dir_id]).child.count(name) > 0){ //existed
        cout << "Invalid operation\n";
        return;
    }
    file newfile;
    if (type == "txt") {
        newfile.type = txt;
        newfile.text = content;
    }
    else if (type == "md") {
        newfile.type = md;
        newfile.text = content;
    }
    else {
        newfile.type = bin;
        newfile.content = split(content);
    }
    datas.push_back(newfile);
    get<dir>(datas[cur_dir_id]).child.insert({name, datas.size() - 1});
    undo_stack.push_back(do_make{cur_dir_id,(int)(datas.size() - 1),name});
}
bool file_check(const string &name, int mode){ 
    //check 1: whether exist such a name
    //check 2: whether a file or not
    //check 3: mode = 0 expect to be txt or md; mode = 1 expect to be bin
    if (get<dir>(datas[cur_dir_id]).child.count(name) == 0) 
        return false;
    int target_id =get<dir>(datas[cur_dir_id]).child[name];
    if (holds_alternative<dir>(datas[target_id]))
        return false; 
    if (get<file>(datas[target_id]).type == bin) {
        if (mode == 0) return false;
        return true;
    }
    else{
        if (mode == 1) return false;
        return true;
    }
}
void append(){
    string name,content;
    cin >> name;
    getline(cin,content);
    newcontent(content);
    if (!file_check(name, 0)){
        cout << "Invalid operation\n";
        return;
    }
    int target_id =get<dir>(datas[cur_dir_id]).child[name];
    file &target = get<file>(datas[target_id]);// & is important, or changing target won't make a real change !
    target.text += content;
    undo_stack.push_back(do_append{target_id,content});
}
void cat(){
    string name;
    cin >> name;
    if (!file_check(name, 0)){
        cout << "Invalid operation\n";
        return;
    }
    int target_id =get<dir>(datas[cur_dir_id]).child[name];
    cout << name << ":" << get<file>(datas[target_id]).text << "\n";
}
const string hex_list = "0123456789ABCDEF";
void read(){
    string name;
    int offset, length;
    cin >> name >> offset >> length;
    if (!file_check(name, 1)) {
        cout << "Invalid operation\n";
        return;
    }
    int target_id =get<dir>(datas[cur_dir_id]).child[name];
    vector<unsigned char>&cont = get<file>(datas[target_id]).content;
    if (offset + length > cont.size()) {
        cout << "Invalid operation\n";
        return;
    }
    string ans = "";
    for (int i = offset; i < offset + length; i++) {
        if (i != offset) ans += " ";
        ans += hex_list[cont[i] >> 4];
        ans += hex_list[cont[i] & 0x0F];
        // seem that it's forbidden to print a "" in the end !
    }
    cout << name << ":" << ans << "\n";
}
void write(){
    string name,hexdata;
    int offset;
    cin >> name >> offset;
    getline(cin, hexdata);
    newcontent(hexdata);
    vector<unsigned char>newcont = split(hexdata);
    if (!file_check(name, 1)){
        cout << "Invalid operation\n";
        return;
    }
    vector<unsigned char>prehex;
    int target_id =get<dir>(datas[cur_dir_id]).child[name];
    file &target = get<file>(datas[target_id]);
    int ed = target.content.size();
    int prelen = ed;
    if (offset >= ed){ // add 00 and then write
        target.content.resize(offset, 0);// ed~offset -> 00
        target.content.insert(target.content.end(),newcont.begin(),newcont.end());
    }
    else {
        int i = offset, j = 0;
        while (i < ed && j < newcont.size()) { 
            prehex.push_back(target.content[i]);
            target.content[i] = newcont[j];
            i++;
            j++;
        }
        target.content.insert(target.content.end(),newcont.begin() + j, newcont.end());
    }
    undo_stack.push_back(do_write{target_id,offset,prelen,newcont,prehex});
}
void ls(){
    auto st = get<dir>(datas[cur_dir_id]).child.begin();
    auto ed = get<dir>(datas[cur_dir_id]).child.end();
    string ans = "";
    for (auto it = st; it != ed; it++){
        int temp_id = it->second;
        if (holds_alternative<dir>(datas[temp_id])) {
            ans += it->first + "/\n";
        }
        else {
            ans += it->first + "\n";
        }
    }
    cout << ans;
}
vector<string> is_r(const string &line){
    vector<string>tok;
    string word="";
    for (char ch : line){
        if (ch==' '){
            if (!word.empty()){
                tok.push_back(word); 
                word = "";
            }
        }
        else{
            word+=ch;
        }
    }
    if (!word.empty()){
        tok.push_back(word);
    }
    return tok;
}
void rm() {
    string line;
    getline(cin,line);
    vector<string> tok = is_r(line);
    string name;
    if (tok.size() == 2)
        name = tok[1];
    else name = tok[0];
    if (get<dir>(datas[cur_dir_id]).child.count(name) == 0) {
        cout << "Invalid operation\n";
        return;
    }
    if (holds_alternative<dir>(datas[get<dir>(datas[cur_dir_id]).child[name]])) {
        if (tok.size() == 1) {
            cout << "Invalid operation\n";
            return;
        }
    }
    int rm_id = get<dir>(datas[cur_dir_id]).child[name];
    undo_stack.push_back(do_rm{cur_dir_id,rm_id,name});
    get<dir>(datas[cur_dir_id]).child.erase(name);
}
int cal_size (int target_id){ // assure that target is a file (it's meaningless to cal the size of dir)
    Type which = get<file>(datas[target_id]).type;
    if (which == bin) 
        return get<file>(datas[target_id]).content.size();
    else 
        return get<file>(datas[target_id]).text.size();
}
void stat(){
    string name;
    cin >> name;
    if (get<dir>(datas[cur_dir_id]).child.count(name) == 0) {
        cout <<"Invalid operation\n";
        return;
    }
    int target_id = get<dir>(datas[cur_dir_id]).child[name];
    if (holds_alternative<dir>(datas[target_id])) 
        cout << name << ":dir:0\n"; 
    else {
        int sizeis = cal_size(target_id);
        Type which = get<file>(datas[target_id]).type;
        if (which == txt) 
            cout << name << ":txt:" << sizeis << "\n";
        else if (which == md)
            cout << name << ":md:" << sizeis << "\n";
        else {
            cout << name << ":bin:" << sizeis << "\n";
        }
    }
}
struct memo{
    string name;
    int size;
};
struct cmp{
    bool operator()(const memo &var1, const memo &var2) const {
        if (var1.size != var2.size)
            return var1.size > var2.size;
        return var1.name < var2.name;
    }
};
// avoid cal in the cmp, it may lead to TLE (each insert need repetitive cal)
void top(){
    int k;
    cin >> k;
    set<memo,cmp>select;
    auto st = get<dir>(datas[cur_dir_id]).child.begin();
    auto ed = get<dir>(datas[cur_dir_id]).child.end();
    for (auto it = st; it != ed; it++){
        if (holds_alternative<dir>(datas[it->second]))
            continue;
        memo cur;
        cur.name = it->first;
        cur.size = cal_size(it->second);
        if (select.size() < k)
            select.insert(cur);
        else {
            const memo &last = *select.rbegin(); // the last element 
            if (cur.size < last.size || (cur.size == last.size && cur.name > last.name)){
                continue;
            }
            select.insert(cur);
            select.erase(prev(select.end()));
        }
    }
    if (k > select.size()) k = select.size();
    for (auto it = select.begin(); k != 0; it++) {
        cout << it->name << ":" << it->size << "\n";
        k--;
    }
}
void undo(){
    if (undo_stack.empty()){
        cout <<"Invalid operation\n";
        return;
    }
    auto &cmd = undo_stack.back();
    if (holds_alternative<do_make>(cmd)){
        auto &temp = get<do_make>(cmd);
        get<dir>(datas[(temp.fid)]).child.erase(temp.variant_name);
    }
    else if (holds_alternative<do_append>(cmd)) {
        string &change_text = get<file>(datas[get<do_append>(cmd).file_id]).text;
        change_text.resize(change_text.size() - get<do_append>(cmd).content.size());
        //substr < resize 
    }
    else if (holds_alternative<do_cd>(cmd)) {
        cur_dir_id = get<do_cd>(cmd).pre_id;
    }
    else if (holds_alternative<do_rm>(cmd)) {
        auto &temp = get<do_rm>(cmd);
        get<dir>(datas[temp.fid]).child.insert({temp.variant_name,temp.variant_id});
    }
    else {
        auto &temp = get<do_write>(cmd);
        vector<unsigned char> &change_content = get<file>(datas[temp.file_id]).content;
        if (temp.offset < temp.prelen) {
            int ed = min((int)(temp.offset + temp.newhexdata.size()), temp.prelen);
            for (int i = temp.offset; i < ed; i++) {
                change_content[i] = temp.prehexdata[i - temp.offset];
            }
        }
        change_content.resize(temp.prelen);// remember to delete the surplus char
    }
    undo_stack.pop_back();
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    //init
    dir init;
    init.fid = -1;
    init.child = {};
    datas.push_back(init); // 疑似取名data出错
    cur_dir_id = 0;

    int n;
    cin >> n;
    for (int i = 0;i < n; i++) {
        string command;
        cin >> command;
        if      (command == "mkdir")    mkdir();
        else if (command == "cd")       cd();
        else if (command == "create")   create();
        else if (command == "append")   append();
        else if (command == "cat")      cat();
        else if (command == "read")     read();
        else if (command == "write")    write();
        else if (command == "ls")       ls();
        else if (command == "rm")       rm();
        else if (command == "stat")     stat();
        else if (command == "top")      top();
        else if (command == "undo")     undo();
    }
}

/* 与AI对话记录
*/

/* 1. 为什么此处data pushback标红显示问题
`data.push_back(init);` 在 C++17 下**理论上应该能编译**，因为 `init` 是 `dir`，而 `variant<dir,file>` 恰好有一个 alternative 是 `dir`，隐式转换是合法的。

标红大概率不是“语法真错”，而是环境/IDE 的误报或标准版本问题。常见原因按可能性排：

---

## 1. 编译标准不是 C++17

`std::variant` 是 C++17 才有的。如果 IDE 或编译命令用的是 C++14/C++11：

- `vector<variant<dir,file>>` 本身就会报错；
- `push_back` 处也会连带标红。

检查：

- g++/clang++ 编译加 `-std=c++17`；
- VS Code 的 `c_cpp_properties.json` 里 `cppStandard` 改成 `c++17`；
- CMake 里 `set(CMAKE_CXX_STANDARD 17)`；
- Visual Studio 项目属性里“C++ 语言标准”选 `/std:c++17`。

---

## 2. IntelliSense / clangd 对 `variant` 隐式转换误报

`std::variant` 的转换构造函数模板比较重，某些版本的 IntelliSense 或 clangd 会误判“没有匹配的 push_back”。

可以显式构造，绕开隐式转换：

```cpp
data.push_back(std::variant<dir, file>(init));
```

或者：

```cpp
data.emplace_back(std::in_place_type<dir>, init);
```

如果显式写之后不标红了，基本就是 IDE 误报，实际编译没问题。

---

## 3. 检查 `bin` 是否与宏冲突

极少数平台头文件里 `bin` 可能是宏。可以临时改名验证：

```cpp
enum Type { TXT, MD, BIN };
```

如果改名后不标红了，就是宏冲突。

---

## 4. 顺带指出：`main` 里 `command` 没读入

你这段：

```cpp
for (int i = 0;i < n; i++) {
    string command;
    if (command == "mkdir") mkdir();
}
```

`command` 从未 `cin >> command`，永远是空串，所有指令都不会执行。应该是：

```cpp
for (int i = 0; i < n; ++i) {
    string command;
    cin >> command;
    if (command == "mkdir") mkdir();
    else if (command == "cd") cd();
    ...
}
```

---

## 5. 更建议：不要用 `variant<dir,file>`，统一成一个结构体

你注释里写“只用一个结构体”，但代码里还是 `dir` / `file` 两个类型加 `variant`。既然题目里目录和文件共用名字空间、都要放在 `map<string,int>` 里，其实统一成一个 `Node` 更省事：

```cpp
struct Node {
    bool isDir;
    int parent;
    Type type;              // 仅文件有效
    std::string text;       // txt/md
    std::vector<unsigned char> bin; // bin
    std::map<std::string, int> child; // 仅目录有效，有序
};
std::vector<Node> nodes;
```

这样：

- 不需要 `variant`，避开隐式转换和 IntelliSense 的坑；
- `map<string,int>` 天然按名字字典序，`ls`/`tree` 直接遍历；
- 目录的 `child` 和文件的 `content` 分开字段，逻辑清楚；
- `rm -r` 递归删除也方便。

---

## 结论

`push_back(init)` 标红**不是 C++17 下的真实语法错误**，优先检查：

1. 是否真的开了 `-std=c++17`；
2. 是否只是 IntelliSense 误报，显式 `variant<dir,file>(init)` 或 `emplace_back(in_place_type<dir>, init)` 试试；
3. 是否 `bin` 名字冲突。

另外记得补上 `cin >> command`，否则主循环不会执行任何指令。
*/

/*2. ?但是vector<dir>data;就算改成这样还是报错.而且variant目前没报错。json脚本是
{
    // ===== 核心分离：明确分工 =====
    "C_Cpp.intelliSenseEngine": "disabled",
    "C_Cpp.autocomplete": "Disabled",
    "C_Cpp.errorSquiggles": "Disabled",

    // ===== clangd 接管代码分析 =====
    "clangd.fallbackFlags": [
        "-std=c++17"
    ],
    "[cpp]": {
        "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"
    },
    "[c]": {
        "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"
    },

    // ===== C/C++ 扩展保留：调试与 Code Runner =====
    "C_Cpp.default.compilerPath": "/usr/bin/clang++",
    "C_Cpp.default.cppStandard": "c++17",
    "C_Cpp.default.cStandard": "c17",
    "C_Cpp.default.intelliSenseMode": "macos-clang-arm64",

    "code-runner.runInTerminal": true,
    "code-runner.preserveFocus": false,
    "code-runner.executorMap": {
        "cpp": "cd $dir && clang++ -std=c++17 $fileName -o $fileNameWithoutExt && ./$fileNameWithoutExt"
    },

    // ===== 你原有的界面与终端设置（保持不变）=====
    "terminal.integrated.enableMultiLinePasteWarning": "never",
    "terminal.integrated.initialHint": false,
    "editor.tokenColorCustomizations": {
        "textMateRules": [
            {
                "scope": "punctuation.definition.list.begin.markdown",
                "settings": { "foreground": "#ffcc00" }
            },
            {
                "scope": "markup.heading entity.name.section.markdown",
                "settings": { "foreground": "#47ff1e" }
            },
            {
                "scope": "markup.list.numbered.markdown",
                "settings": { "foreground": "#00ffff" }
            }
        ]
    },
    "workbench.colorTheme": "Dark+",

    // 之前为了抑制弹窗加的，现在可以保留，但已不是关键
    "clangd.detectExtensionConflicts": false
}
你这段代码里 `data.push_back(init)` 标红，**大概率不是 C++ 语法错误**，而是 clangd 的解析/缓存/名字冲突问题。尤其是你说“改成 `vector<dir> data;` 也报错”，那就基本排除了 `std::variant` 隐式转换的问题，问题更可能出在 `dir` / `data` 这两个名字或 clangd 环境上。

先做三件事确认：

---

## 1. 先别信红波浪线，直接用命令行编译

在终端里执行：

```bash
clang++ -std=c++17 -Wall -Wextra main.cpp -o main
```

或者：

```bash
g++ -std=c++17 -Wall -Wextra main.cpp -o main
```

看真实报错。  
如果命令行能编译通过，那就是 clangd 误报/缓存问题，不是代码错。

---

## 2. 如果命令行也报错，把具体错误贴出来

现在只知道“标红”，不知道报错文本。常见真实错误可能是：

- `dir` 与系统头文件里的名字冲突；
- `data` 与某个宏或名字冲突；
- `Type` 里的 `bin` 与宏冲突；
- `map<string,int> child;` 里 `child` 与某宏冲突。

但你的代码里 `variant` 没报错，说明 `#include <variant>` 和 `std::variant` 是识别了的，所以不是没开 C++17。

---

## 3. 最可疑：`dir` 这个名字

在 macOS 上，`dir` 虽然不是标准 C++ 关键字，但某些系统头文件或宏可能定义过 `dir`。  
建议直接改名验证：

```cpp
struct Dir {
    int fid;
    std::map<std::string, int> child;
};

struct File {
    int fid;
    Type type;
    std::string text;
    std::vector<char> content;
};
```

然后：

```cpp
std::vector<std::variant<Dir, File>> data;
```

如果改名后不红了，就是名字冲突。

同理，`data` 也改成 `nodes`：

```cpp
std::vector<std::variant<Dir, File>> nodes;
```

`bin` 也建议改成 `BIN` 或 `Bin`，避免与某些宏冲突。

---

## 4. 清理 clangd 缓存

clangd 有时会缓存旧索引，改代码后仍标红。可以：

1. 命令面板执行 `clangd: Restart language server`；
2. 删除项目下的 `.cache/clangd`；
3. 确保文件已保存；
4. 确认 clangd 用的是 `-std=c++17`。

你的配置里：

```json
"clangd.fallbackFlags": ["-std=c++17"]
```

这个只对没有 `compile_commands.json` 的文件生效。如果项目里有 `compile_commands.json`，clangd 会优先用它。可以临时在项目根目录放一个空的 `compile_commands.json` 或者确认它包含你的文件。

---

## 5. 另外，你 `main` 里漏了读命令

这段：

```cpp
for (int i = 0;i < n; i++) {
    string command;
    if (command == "mkdir") mkdir();
}
```

`command` 从未读入，永远是空串，所有指令都不会执行。应该是：

```cpp
for (int i = 0; i < n; ++i) {
    std::string command;
    std::cin >> command;

    if (command == "mkdir") mkdir();
    else if (command == "cd") cd();
    else if (command == "create") create();
    // ...
}
```

---

## 6. 更建议的结构

你注释里写“只用一个结构体”，但代码还是 `dir` / `file` + `variant`。其实统一成一个 `Node` 更简单，也能避开 `variant` 和名字冲突：

```cpp
struct Node {
    bool isDir = false;
    int parent = -1;

    Type type;                    // 文件有效
    std::string text;             // txt/md
    std::vector<unsigned char> bin; // bin

    std::map<std::string, int> child; // 目录有效，按名字排序
};

std::vector<Node> nodes;
```

这样：

- 不需要 `variant`；
- `ls` / `tree` 直接遍历 `child`，天然字典序；
- `stat` 判断 `isDir` 即可；
- `rm -r` 递归删除也方便；
- 不会出现 `dir` / `file` 名字冲突。

---

## 结论

`data.push_back(init)` 标红优先按这个顺序排查：

1. 命令行 `clang++ -std=c++17` 编译，看是否真错；
2. 把 `dir` 改成 `Dir`、`file` 改成 `File`、`data` 改成 `nodes`、`bin` 改成 `BIN`；
3. 重启 clangd / 删 `.cache/clangd`；
4. 看“问题”面板的具体错误文本。

如果命令行编译通过，就是 clangd 误报，不用管红波浪线。
*/

/*3.variant<var1,var2> 怎么返回一个我已经确定类型是var1的变量
比如此处
struct dir{
    int fid; // father dir's id
    map<string, int> child; // children 's name & id.sorted
    // remember that file and dir cannot share the same name
};
struct file{
    int fid; // father dir's id
    Type type;
    string text;// for the txt & md
    vector<char> content;// for the bin
};
vector<variant<dir,file>>datas;
int cur_dir_id = 0;
假设我已经知道cur_dir_id（而且对应的是文件夹），怎么写可以得到那个child map

## 核心答案：用 `std::get<dir>`

既然你已经确定这个 variant 里装的是 `dir`（也就是 `var1`），直接 `std::get<dir>` 取引用即可；`std::get` 在类型不匹配时会抛 `std::bad_variant_access`，属于"我确定"的场景。

```cpp
variant<dir, file>& v = datas[i];
dir& d = std::get<dir>(v);        // 按类型取
// 或
dir& d2 = std::get<0>(v);         // 按索引取，var1 是 0
// d.child 就是你要的 map
```

如果不想依赖异常，用指针版 `get_if`：

```cpp
if (auto* p = std::get_if<dir>(&v)) {
    // p->child 是 map<string,int>&
}
```

或者先判断再取：

```cpp
if (std::holds_alternative<dir>(v)) {
    dir& d = std::get<dir>(v);
}
```

## 针对你的场景

你想"根据 `cur_dir_id` 找到那个 dir 的 `child` map"，按你现在定义的结构有个问题：

```cpp
struct dir{
    int fid;              // 父目录 id
    map<string,int> child;
    // ⚠️ 没有存"自己"的 id
};
```

`dir` 里只有 `fid`（父 id），**没有自己的 id**。所以没法直接凭 `cur_dir_id` 在 `datas` 里匹配出这个 dir 本身。要能这样查，需要满足下面之一：

**方案 1：给 dir 加自己的 id**（推荐，最直观）

```cpp
struct dir{
    int id;               // 自己的 id
    int fid;
    map<string,int> child;
};
```

然后：

```cpp
dir* find_dir(int id) {
    for (auto& v : datas) {
        if (auto* p = std::get_if<dir>(&v))
            if (p->id == id) return p;
    }
    return nullptr;
}

// 用法
if (dir* d = find_dir(cur_dir_id)) {
    auto& m = d->child;   // 这就是 map<string,int>&
}
```

**方案 2：约定 `datas[i]` 的目录 id 就是 `i`**（省空间但耦合强）

```cpp
// 前提：每个目录在 datas 里的下标就是它的 id
dir& d = std::get<dir>(datas[cur_dir_id]);
auto& m = d->child;
```

**方案 3：通过父目录的 `child` map 反查**
先在父目录的 `child` 里找到名字，再拿 id 去 `datas[id]`。但既然都拿到 id 了，本质上还是方案 2。

## 小建议

- 用 `std::get_if` / `holds_alternative` 做防御，比你想象中少踩坑（比如后续误把 `file` 塞进同名 id）。
- 如果访问很频繁，遍历 `vector` 找 id 是 O(n)，可以考虑额外维护 `unordered_map<int, size_t> id2idx;` 把 id 映射到 `datas` 下标，然后把 `datas` 换成 `vector` + 索引，或直接 `unordered_map<int, variant<dir,file>>`。
- 注意 `std::get<dir>(v)` 返回的是引用，别把它赋给 `auto`（会拷贝整个 `dir`，连 map 一起），用 `auto&` 或 `dir&`。
*/

/*4.怎么判断遍历到的变量是不是某个类型，比如我自己定义了variant 怎么判断当前是类型var1还是var2 c++
# C++ 中判断 variant 当前是哪个类型

要看你说的"自己定义了 variant"是哪种形式，几种常见情况分别说：

---

## 一、如果用的是 `std::variant`（C++17）

假设：

```cpp
#include <variant>
#include <string>

struct Var1 { int a; };
struct Var2 { double b; };

using MyVariant = std::variant<Var1, Var2>;
```

### 方法 1：`holds_alternative`（最简单，适合 if-else）

```cpp
for (const auto& v : vec) {
    if (std::holds_alternative<Var1>(v)) {
        // ...
    } else if (std::holds_alternative<Var2>(v)) {
        // ...
    }
}
```

### 方法 2：`get_if`（推荐，安全 + 只检查一次）

`std::get<T>` 类型不匹配会抛 `std::bad_variant_access`，用 `get_if` 返回指针更安全，而且比 `holds_alternative` + `get` 少一次判断：

```cpp
for (auto& v : vec) {
    if (auto* p = std::get_if<Var1>(&v)) {
        use(p->a);
    } else if (auto* p = std::get_if<Var2>(&v)) {
        use(p->b);
    }
}
```

### 方法 3：`std::visit`（**最推荐**，编译期强制覆盖所有分支）

```cpp
std::visit([](auto&& arg) {
    using T = std::decay_t<decltype(arg)>;   // 去掉 const/&
    if constexpr (std::is_same_v<T, Var1>) {
        use(arg.a);
    } else if constexpr (std::is_same_v<T, Var2>) {
        use(arg.b);
    }
}, v);
```

更惯用的写法是用 `overloaded` 技巧（一个"重载集"）：

```cpp
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;   // C++17 CTAD

for (const auto& v : vec) {
    std::visit(overloaded{
        [](const Var1& x) { use(x.a); },
        [](const Var2& x) { use(x.b); },
    }, v);
}
```

**`visit` 的优点**：编译器会检查你是否覆盖了 variant 里所有类型，将来加新类型会编译报错，比手写 if-else 更不易漏。

---

## 二、如果是你自己写的 tagged union

```cpp
struct MyVariant {
    enum class Tag { Var1, Var2 } tag;
    union { Var1 v1; Var2 v2; };   // 或 std::aligned_union / 成员共存
};
```

直接判断 tag 即可：

```cpp
switch (mv.tag) {
    case MyVariant::Tag::Var1: use(mv.v1); break;
    case MyVariant::Tag::Var2: use(mv.v2); break;
}
```

建议把 tag 和取值的操作封装成 `is_var1()` / `as_var1()` / `visit()` 等方法，避免手写时忘记判 tag 导致访问错误联合体成员（UB）。

---

## 三、如果是多态基类（`Var1`、`Var2` 都继承自基类）

```cpp
for (Base* p : vec) {
    if (auto* a = dynamic_cast<Var1*>(p)) {
        // ...
    } else if (auto* b = dynamic_cast<Var2*>(p)) {
        // ...
    }
}
```

`dynamic_cast` 失败返回 `nullptr`（对指针）；如果 `Var2` 是 final 且非多态，用 `typeid` 比较也可以，但性能/可维护性一般不如虚函数。

---

## 四、遍历时容易踩的坑

1. **`std::get<T>` 会抛异常**——遍历时若不保证类型一定匹配，用 `get_if`。
2. **`visit` 里注意 `const`/引用修饰**——用 `std::decay_t<decltype(arg)>` 归一化再 `is_same_v`，否则 `const Var1&` 和 `Var1` 判不相等。
3. **`std::is_same_v<T, Var1>` 是编译期的**，所以必须配 `if constexpr`，不能配普通 `if`。
4. **`holds_alternative` 后接 `get` 会检查两次**，性能敏感处直接用 `get_if`。
5. **不确定 variant 是否 `valueless_by_exception()`**——极端情况下（赋值时类型构造抛异常）variant 可能无值，`visit` 会抛 `bad_variant_access`，一般业务里很少遇到。

---

## 一句话总结

| 你的"variant"形式 | 判断方式 |
|---|---|
| `std::variant` | `holds_alternative` / `get_if` / **`std::visit`（推荐）** |
| 自己写的 union + tag | 读 `tag` 枚举 |
| 多态基类 | `dynamic_cast` |

如果是遍历容器并对每个元素分派处理，`std::visit` + `overloaded` 是最干净、最不易漏分支的写法。
*/

/*5. c++ int是不是可以直接转字符？那如果16进制呢，c++有没有自带16进制转int
分几个概念说清楚：

## 1. `int` 能不能直接转 `char`？

可以写：

```cpp
int i = 65;
char c = i;          // 隐式窄化转换，c 通常是 'A'
char c2 = static_cast<char>(i);
```

但注意：

- 这不是“把整数转成十进制数字字符串”，而是把整数当成字符编码值。
- 如果 `i` 超出 `char` 能表示的范围，结果依赖实现，可能截断。
- 如果想把数字 `0~9` 变成字符 `'0'~'9'`：

```cpp
int d = 7;
char c = '0' + d;    // '7'，仅适用于 d 在 0~9
```

- 如果想把整数 `123` 变成字符串 `"123"`，用：

```cpp
std::string s = std::to_string(123);
```

## 2. 16 进制转 `int`，C++ 有自带的

有，常用三种。

### 方法一：`std::stoi`，最简单

```cpp
#include <string>
#include <iostream>

int main() {
    std::string s = "1A";
    int x = std::stoi(s, nullptr, 16);  // 26
    std::cout << x << '\n';

    std::string s2 = "0xFF";
    int y = std::stoi(s2, nullptr, 16); // 255，通常可识别 0x 前缀
    std::cout << y << '\n';
}
```

注意：`stoi` 解析失败会抛异常，超范围也抛异常。

### 方法二：`std::strtol`

```cpp
#include <cstdlib>
#include <iostream>

int main() {
    const char* s = "1A";
    char* end = nullptr;
    long v = std::strtol(s, &end, 16);
    std::cout << v << '\n';  // 26
}
```

适合 C 风格字符串，错误处理看 `end` 和 `errno`。

### 方法三：C++17 `std::from_chars`，推荐用于严格解析

```cpp
#include <charconv>
#include <string>
#include <iostream>

int main() {
    std::string s = "1A";
    int x = 0;
    auto result = std::from_chars(s.data(), s.data() + s.size(), x, 16);

    if (result.ec == std::errc{}) {
        std::cout << x << '\n';  // 26
    }
}
```

注意：`from_chars` 的 base=16 模式下**不识别 `0x` 前缀**，要自己去掉。

## 3. 反过来：`int` 转 16 进制字符串

C++17 可用 `std::to_chars`：

```cpp
#include <charconv>
#include <iostream>

int main() {
    int x = 255;
    char buf[32];
    auto result = std::to_chars(buf, buf + sizeof(buf), x, 16);
    *result.ptr = '\0';
    std::cout << buf << '\n';  // ff
}
```

也可以用：

```cpp
#include <sstream>
#include <iomanip>

std::stringstream ss;
ss << std::hex << 255;
std::string s = ss.str();  // "ff"
```

或 `printf("%x", x)` / `std::format("{:x}", x)`（C++20）。

## 总结

- `int` 可以隐式转 `char`，但只是按字符编码截断，不是“数字转字符串”。
- 数字 `0~9` 转字符：`'0' + d`。
- 整数转十进制字符串：`std::to_string`。
- 16 进制字符串转 `int`：用 `std::stoi(s, nullptr, 16)` 最方便；严格解析用 `std::from_chars(..., 16)`。
- `int` 转 16 进制字符串：用 `std::to_chars(..., 16)`、`std::hex` 或 `printf("%x")`。
*/

/*6. 那FF会炸吗
分情况看，“FF”本身不会炸，但有几个容易踩的坑。

## 1. `"FF"` 字符串转 `int`：不会炸

```cpp
int x = std::stoi("FF", nullptr, 16);  // 255
```

`FF` 十六进制 = 255，远在 `int` 范围内（通常最大 2147483647），完全没问题。

## 2. 真正会“炸”的是数值超范围

```cpp
int x = std::stoi("FFFFFFFF", nullptr, 16);  // 4294967295，超出 int
```

- `std::stoi` 会抛 `std::out_of_range` 异常 —— 这就是“炸”。
- `std::from_chars` 不抛异常，返回 `result.ec == std::errc::result_out_of_range`，需要自己检查。
- `std::strtol` 返回 `LONG_MAX` 并设置 `errno == ERANGE`。

所以要看你的输入可能有多大。如果可能超过 `int` 范围，用 `long long` 或 `std::stoll` / `from_chars` 到 `long long`：

```cpp
long long v = std::stoll(s, nullptr, 16);  // 能装下 0xFFFFFFFF
```

## 3. 另一个常见的“炸”：`0xFF` 赋给 `char`

```cpp
char c = 0xFF;
```

- 如果 `char` 是 **signed**（x86/ARM 常见），`0xFF` = 255 超出 `[-128,127]`，结果**实现定义**，通常变成 `-1`。
- 如果 `char` 是 **unsigned**，结果是 255。
- 这不会崩，但结果反直觉。要明确用 `unsigned char` 或 `std::uint8_t`：

```cpp
unsigned char c = 0xFF;  // 一定是 255
```

## 4. 还有 `"0xFF"` 前缀的差异

```cpp
std::stoi("0xFF", nullptr, 16);  // 通常能解析成 255（识别 0x）
std::from_chars(s.data(), s.data()+s.size(), x, 16);  // 不识别 0x，会停在前缀处
```

用 `from_chars` 时记得自己去掉 `0x`。

## 结论

- `"FF"` 转 `int` = 255，**不会炸**。
- 会炸的是**超出 int 范围的输入**，用 `stoll` 或检查 `from_chars` 的返回码。
- 另一个坑是 `0xFF` 赋给 signed `char` 会变成 `-1`，用 `unsigned char` 避免。

*/

/* 7. 为什么报错select.erase(select.rbegin()); select是set
报错的原因是 **`std::set::erase` 没有接受反向迭代器的重载版本**。`select.rbegin()` 返回的是 `std::set<T>::reverse_iterator`，而 `erase` 成员函数期望的参数类型是普通的 `iterator`（或 `const_iterator`）。两者类型不匹配，因此编译器找不到可用的 `erase` 函数。

### 正确的写法

如果想删除 `set` 的最后一个元素，应该使用正向迭代器：

```cpp
select.erase(--select.end());
```

或者更安全地使用 `std::prev`（需要包含 `<iterator>`）：

```cpp
select.erase(std::prev(select.end()));
```

这两种写法都能得到指向最后一个元素的正向迭代器，符合 `erase` 的参数要求。

### 为什么 `rbegin().base()` 也不行

一个常见的误解是认为 `reverse_iterator` 的 `base()` 方法可以“转换”回普通迭代器。但 **`rbegin().base()` 返回的迭代器指向的是最后一个元素的“下一个”位置（即 `end()`），而不是最后一个元素本身**。直接把它传给 `erase` 会导致未定义行为（可能崩溃），因为它试图删除尾后迭代器。
*/

/* 8. c++ size_t是什么
`size_t` 是 C++ 标准库中定义的**无符号整数类型**，用来表示“大小、长度、计数、下标”这类非负值。

要点：

1. **定义位置**
   - C++ 中通常用 `std::size_t`，定义在 `<cstddef>`（也常通过其他头文件间接引入）。
   - C 风格的全局 `size_t` 定义在 `<stddef.h>` / `<cstddef>` 中。

2. **本质**
   - 它不是关键字，而是标准库定义的整数类型别名/`typedef`。
   - 具体底层类型由平台决定：
     - 32 位平台常见是 `unsigned int`
     - 64 位平台常见是 `unsigned long` 或 `unsigned long long`
   - 标准保证它能表示任何对象的大小。

3. **典型用途**
   ```cpp
   size_t n = sizeof(int);        // sizeof 的返回类型就是 size_t
   std::vector<int> v(10);
   for (size_t i = 0; i < v.size(); ++i) { ... }
   ```

4. **常见注意点**
   - 它是**无符号类型**，和有符号 `int` 比较时可能发生隐式转换，导致意外结果：
     ```cpp
     int i = -1;
     size_t n = 10;
     if (i < n) { ... } // i 可能被转换成很大的无符号数
     ```
   - 倒序循环要小心：
     ```cpp
     for (size_t i = v.size(); i-- > 0; ) { ... }
     ```
   - `printf` 输出 `size_t` 用 `%zu`：
     ```cpp
     printf("%zu\n", sizeof(int));
     ```

5. **和其他类型区别**
   - `size_t`：表示大小/长度，无符号。
   - `ptrdiff_t`：表示两个指针差值，有符号。
   - `uintptr_t`：能存放指针值的无符号整数类型。

简单说：**`size_t` 就是标准库提供的、用于表示对象大小和容器长度的无符号整数类型，`sizeof` 和容器的 `size()` 都返回它。**
*/

/*9. ios::sync_with_stdio(false);cin.tie(nullptr);这是用来干嘛的
这两行代码是 C++ 算法竞赛和性能敏感场景中的标准“加速”写法，核心目的是**解除 `cin`/`cout` 因兼容性和绑定关系而承受的额外开销**，从而大幅提升输入输出（I/O）效率。

### `ios::sync_with_stdio(false);`：关闭与 C 标准流的同步

**作用**
关闭 C++ 标准流（`cin`/`cout`）与 C 标准流（`scanf`/`printf`）之间的同步。

**原理与性能影响**
默认情况下，C++ 为了兼容 C，保证你混用 `cout` 和 `printf` 时输出顺序不乱，会让它们共享缓冲区并保持同步。这意味着**每一次 C++ 流的 I/O 操作，都会立即被同步到 C 的流缓冲区中**，C++ 流自身相当于“无缓冲”状态，开销很大。

关闭同步后，C++ 流拥有了自己独立的缓冲区，可以批量处理 I/O，效率会显著提升。在一些测试中，这能将 `cin`/`cout` 的速度提升数倍，使其与 `scanf`/`printf` 的性能差距大幅缩小。

**关键注意事项**
一旦关闭同步，**就不能再混用 `cin` 和 `scanf`，也不能混用 `cout` 和 `printf`**，否则输入输出的顺序将无法保证。不过，交叉使用（如 `cin` 配 `printf`）通常是可以的，但最安全的做法是统一使用 C++ 流或统一使用 C 风格 I/O。

此外，这个函数**必须在任何 I/O 操作之前调用**。如果在已经进行了输入输出后再调用，其行为是未定义的。

### `cin.tie(nullptr);`：解除 `cin` 与 `cout` 的绑定

**作用**
解除输入流 `cin` 与输出流 `cout` 之间的绑定关系。

**原理与性能影响**
默认情况下，`cin` 被“绑定”（tied）到 `cout`。这意味着**每次你从 `cin` 读取数据之前，程序都会强制刷新 `cout` 的缓冲区**，以确保所有待输出的内容都显示出来。这在交互式程序中可以避免用户看不到提示信息，但在大量读写的场景下，**每一次输入都伴随一次不必要的输出刷新**，会严重拖慢性能。

`cin.tie(nullptr)` 将这个绑定解除。之后，从 `cin` 读取数据不再自动触发 `cout` 刷新，从而省去了大量无谓的刷新开销，进一步提速。

**关键注意事项**
解除绑定后，如果在输出提示信息后需要用户立即看到，**必须手动刷新**。可以使用 `std::flush` 或 `std::endl`（它会自动刷新）。如果只是用 `\n` 换行，信息可能会留在缓冲区中，直到程序结束或缓冲区满时才显示。

### 补充说明

- **`nullptr` vs `0`/`NULL`**：`cin.tie(nullptr)`、`cin.tie(0)`、`cin.tie(NULL)` 在功能上等价，`nullptr` 是 C++11 起更推荐的类型安全写法。
- **无需 `cout.tie(nullptr)`**：默认情况下 `cout` 本身没有被绑定到其他流，所以对 `cout` 调用 `tie(nullptr)` 是多余且无效的。
- **适用场景**：如果你**只使用 C++ 的 `cin`/`cout`**，并且面临大量 I/O（如算法竞赛、数据处理），这两行是标准的优化起手式。如果你的程序以 C 风格 I/O 为主，或者需要严格保证 C/C++ 混用时的顺序，则不应添加。
*/

/*10. 有没有什么办法可以快速删去、替换、加上相同元素到vector<unsigned char>里面，就是怎么避免循环多次到popback\pushback,c++
# 避免循环 push_back / pop_back 的批量操作技巧

针对 `vector<unsigned char>`，核心思路是：**用「填充构造 + 区间 erase / resize」代替逐元素操作**。下面按场景列出来。

## 1. 批量追加相同元素（代替循环 push_back）

```cpp
std::vector<unsigned char> v;

// 追加 n 个 0x00（一次调用，内部按填充构造）
v.insert(v.end(), n, 0x00);

// 或者用 resize 直接扩并填值
v.resize(v.size() + n, 0xFF);

// C++23
v.append_range(std::views::repeat(0xFF, n));
```

`insert(end, count, value)` 是一次性分配 + `memset` 式填充，比循环 `push_back` 快得多（避免多次扩容判断）。

## 2. 批量删除尾部元素（代替循环 pop_back）

```cpp
// 删掉最后 n 个
v.resize(v.size() - n);            // 最快，不涉及元素移动

// 或者
v.erase(v.end() - n, v.end());
```

`resize` 对 POD 类型（`unsigned char`）不会调用析构，就是简单改一下 size，几乎零成本。

## 3. 批量替换相同元素

**把一段区间替换成同一个值：**

```cpp
// 用 fill（对 unsigned char 编译器一般会优化成 memset）
std::fill(v.begin() + start, v.begin() + end, newVal);

// 或直接 memset，字节类型最省事
std::memset(v.data() + start, newVal, end - start);
```

**把所有等于旧值的元素替换成新值：**

```cpp
std::replace(v.begin(), v.end(), oldVal, newVal);
```

## 4. 批量删除等于某值的元素（erase-remove 惯用法）

```cpp
// C++20 最简洁
std::erase(v, value);

// C++20 之前
v.erase(std::remove(v.begin(), v.end(), value), v.end());
```

对于 `unsigned char`，如果只涉及少数几个值，甚至可以用 `std::count` + 手动搬移，但一般 `std::erase` 已足够。

## 5. 删除中间一段区间

```cpp
v.erase(v.begin() + i, v.begin() + j);   // 一次移动完成，不是循环
```

## 6. 批量覆盖/拼接

```cpp
// 把另一段数据拷贝覆盖到中间
std::copy(src.begin(), src.end(), v.begin() + offset);
std::memcpy(v.data() + offset, srcPtr, len);

// 在末尾批量追加另一段
v.insert(v.end(), src.begin(), src.end());
```

## 性能提醒

- **尾部操作**（insert end / resize / erase end）都是均摊 O(1) 或 O(1)，这是 vector 最擅长的。
- **中间插入/删除**是 O(n) 的元素搬移（`memmove`），虽然是一次 `memmove` 而非循环，但如果**频繁在中间增删**，`vector` 本身就不合适，应考虑 `std::deque` / `std::list` / 环形缓冲区等。
- 对 `unsigned char` 这类 POD，编译器通常会把批量填充/搬移优化成 `memset` / `memmove`，所以「一次调用处理整段」的收益非常明显。

## 一句话总结

> 用 `insert(end, n, val)` / `resize` 批量增，用 `resize` / `erase(range)` 批量删，用 `std::fill` / `std::replace` / `std::erase` 批量改 —— **永远不要用循环去逐字节操作，一次 API 调用处理整段。**
*/