#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <variant>
#include <algorithm>

using namespace std;

enum Type {txt,md,bin};
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
struct dir{
    string name;
    int fid; // father dir's id
    map<string, int> child; // children 's name & id.sorted.
    // remember that file and dir cannot share the same name
    set<memo,cmp> select; // for the top[k]
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
    int fid, file_id;
    string name, content;
};
struct do_cd{
    int pre_id, post_id;
};
struct do_write{
    int fid, file_id, offset, prelen;
    string name;
    vector<unsigned char>newhexdata,prehexdata;
};
struct do_rm{
    int fid, variant_id;
    string variant_name;
};
vector<variant<do_make,do_append,do_cd,do_write,do_rm>>undo_stack;
vector<variant<do_make,do_append,do_cd,do_write,do_rm>>redo_stack;

//command
void mkdir(){
    string name;
    cin >> name;
    if (get<dir>(datas[cur_dir_id]).child.count(name) > 0) { 
        cout << "Invalid operation\n";
        return;
    }
    datas.push_back((dir){name,cur_dir_id,{},{}});
    dir &curd = get<dir>(datas[cur_dir_id]); // cur_dir
    curd.child.insert({name , datas.size() - 1});
    redo_stack.clear();
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
    redo_stack.clear();
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
int cal_size (int target_id){ // assure that target is a file (it's meaningless to cal the size of dir)
    Type which = get<file>(datas[target_id]).type;
    if (which == bin) 
        return get<file>(datas[target_id]).content.size();
    else 
        return get<file>(datas[target_id]).text.size();
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
    get<dir>(datas[cur_dir_id]).select.insert({name,cal_size(datas.size() - 1)});
    redo_stack.clear();
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
    get<dir>(datas[cur_dir_id]).select.erase({name,cal_size(target_id)});
    file &target = get<file>(datas[target_id]);// & is important, or changing target won't make a real change !
    target.text += content;
    get<dir>(datas[cur_dir_id]).select.insert({name,cal_size(target_id)});
    redo_stack.clear();
    undo_stack.push_back(do_append{cur_dir_id,target_id,name,content});
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
    get<dir>(datas[cur_dir_id]).select.erase({name,cal_size(target_id)});
    file &target = get<file>(datas[target_id]);
    int ed = target.content.size();
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
    get<dir>(datas[cur_dir_id]).select.insert({name,cal_size(target_id)});
    redo_stack.clear();
    undo_stack.push_back(do_write{cur_dir_id,target_id,offset,ed,name,newcont,prehex});
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
    auto &d = get<dir>(datas[cur_dir_id]);
    int rm_id = d.child[name];
    d.child.erase(name);
    if (holds_alternative<file>(datas[rm_id]))
        d.select.erase({name, cal_size(rm_id)});
    redo_stack.clear();
    undo_stack.push_back(do_rm{cur_dir_id,rm_id,name});
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
void top(){
    int k;
    cin >> k;
    auto it = get<dir>(datas[cur_dir_id]).select.begin();
    auto ed = get<dir>(datas[cur_dir_id]).select.end();
    string ans = "";
    while (it != ed && k > 0) {
        ans += it->name + ":" + to_string(it->size) +"\n";
        it++;k--;
    }
    cout << ans;
}
void undo(){
    if (undo_stack.empty()){
        cout <<"Invalid operation\n";
        return;
    }
    auto &cmd = undo_stack.back();
    if (holds_alternative<do_make>(cmd)){
        auto &temp = get<do_make>(cmd);
        auto &d =  get<dir>(datas[temp.fid]);
        if (holds_alternative<file>(datas[temp.variant_id])) {
            d.select.erase({temp.variant_name, cal_size(temp.variant_id)});
        }
        d.child.erase(temp.variant_name);
    }
    else if (holds_alternative<do_append>(cmd)) {
        auto &temp = get<do_append>(cmd);
        auto &d = get<dir>(datas[temp.fid]);
        file &f = get<file>(datas[temp.file_id]);
        d.select.erase({temp.name, cal_size(temp.file_id)});
        f.text.resize(f.text.size() - temp.content.size());
        //substr < resize 
        d.select.insert({temp.name, cal_size(temp.file_id)});
    }
    else if (holds_alternative<do_cd>(cmd)) {
        cur_dir_id = get<do_cd>(cmd).pre_id;
    }
    else if (holds_alternative<do_rm>(cmd)) {
        auto &temp = get<do_rm>(cmd);
        auto &d = get<dir>(datas[temp.fid]);
        d.child.insert({temp.variant_name,temp.variant_id});
        if (holds_alternative<file>(datas[temp.variant_id])) {
            d.select.insert({temp.variant_name, cal_size(temp.variant_id)});
        }
    }
    else {
        auto &temp = get<do_write>(cmd);
        auto &d = get<dir>(datas[temp.fid]);
        file &f = get<file>(datas[temp.file_id]);
        vector<unsigned char> &change_content = f.content;
        d.select.erase({temp.name, cal_size(temp.file_id)});
        if (temp.offset < temp.prelen) {
            int ed = min((int)(temp.offset + temp.newhexdata.size()), temp.prelen);
            for (int i = temp.offset; i < ed; i++) {
                change_content[i] = temp.prehexdata[i - temp.offset];
            }
        }
        change_content.resize(temp.prelen);// remember to delete the surplus char
        d.select.insert({temp.name,cal_size(temp.file_id)});
    }
    redo_stack.push_back(std::move(cmd));
    undo_stack.pop_back();
}
void redo(){
    if (redo_stack.empty()){
        cout <<"Invalid operation\n";
        return;
    }
    auto &cmd = redo_stack.back();
    if (holds_alternative<do_make>(cmd)){
        auto &temp = get<do_make>(cmd);
        auto &d = get<dir>(datas[temp.fid]);
        d.child.insert({temp.variant_name,temp.variant_id});
        if (holds_alternative<file>(datas[temp.variant_id])) {
            d.select.insert({temp.variant_name, cal_size(temp.variant_id)});
        }
    }
    else if (holds_alternative<do_append>(cmd)) {
        auto &temp = get<do_append>(cmd);
        auto &d = get<dir>(datas[temp.fid]);
        file &f = get<file>(datas[temp.file_id]);
        d.select.erase({temp.name, cal_size(temp.file_id)});
        f.text += get<do_append>(cmd).content;
        d.select.insert({temp.name, cal_size(temp.file_id)});
    }
    else if (holds_alternative<do_cd>(cmd)) {
        cur_dir_id = get<do_cd>(cmd).post_id;
    }
    else if (holds_alternative<do_rm>(cmd)) {
        auto &temp = get<do_rm>(cmd);
        auto &d =  get<dir>(datas[temp.fid]);
        d.child.erase(temp.variant_name);
        if (holds_alternative<file>(datas[temp.variant_id])) {
            d.select.erase({temp.variant_name, cal_size(temp.variant_id)});
        }
    }
    else {
        auto &temp = get<do_write>(cmd);
        auto &d = get<dir>(datas[temp.fid]);
        file &f = get<file>(datas[temp.file_id]);
        vector<unsigned char> &change_content = f.content;
        d.select.erase({temp.name, cal_size(temp.file_id)});
        if (temp.offset >= temp.prelen) {
            change_content.resize(temp.offset, 0);
            change_content.insert(change_content.end(),temp.newhexdata.begin(),temp.newhexdata.end());
        }
        else {
            int i = temp.offset, j = 0;
            while (i < temp.prelen && j < temp.newhexdata.size()) { 
                change_content[i] = temp.newhexdata[j];
                i++;
                j++;
            }
            change_content.insert(change_content.end(),temp.newhexdata.begin() + j,temp.newhexdata.end());
        }
        d.select.insert({temp.name, cal_size(temp.file_id)});
    }
    undo_stack.push_back(std::move(cmd));
    redo_stack.pop_back();
}
// ├── //  
string TEE = "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 ";
// └── //  
string LAST = "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 ";
// │   //
string VERT = "\xE2\x94\x82   ";
void dfs(const string &pre, int now_dir_id,string &ans){
    auto &d = get<dir>(datas[now_dir_id]);
    int check = d.child.size();
    for (auto it = d.child.begin(); it != d.child.end(); it++,check--){
        if (check == 1){ // end
            if (holds_alternative<file>(datas[it->second])) 
                ans += pre + LAST + it->first + "\n";
            else{
                ans += pre + LAST + it->first + "/\n";
                dfs(pre + "    ", it->second,ans);
            }
        }
        else {
            if (holds_alternative<file>(datas[it->second])) 
                ans += pre + TEE + it->first + "\n";
            else{
                ans += pre + TEE + it->first + "/\n";
                dfs(pre + VERT, it->second,ans);
            }
        }
    }
}
void tree(){
    int now_id = cur_dir_id;
    string ans = get<dir>(datas[now_id]).name;
    if (now_id == 0) ans += "\n";
    else ans += "/\n";
    dfs("",now_id,ans);
    cout << ans;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    //init
    dir init;
    init.fid = -1;
    init.child = {};
    init.name = "/";
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
        else if (command == "redo")     redo();
        else if (command == "tree")     tree();
    }
}