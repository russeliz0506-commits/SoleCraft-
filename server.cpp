#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdlib>
#include <algorithm>
#include "httplib.h"
#include "json.hpp"
using json=nlohmann::json;
using namespace std;
mutex lockdb;
string dbfile="data.json";
json initial(){return {{"orders",json::array()}, {"materials",json::array({{{"name","Leather"},{"stock",100},{"capacity",100},{"unit","sheets"},{"cost",250}},{{"name","Shoe Glue"},{"stock",56},{"capacity",100},{"unit","bottles"},{"cost",80}},{{"name","Soles"},{"stock",30},{"capacity",100},{"unit","pairs"},{"cost",150}},{{"name","Thread"},{"stock",18},{"capacity",100},{"unit","rolls"},{"cost",45}}})},{"supplies",json::array()},{"defects",json::array()}};}
json load(){ifstream f(dbfile);if(!f.good())return initial();try{json j;f>>j;return j;}catch(...){return initial();}}
void save(const json& j){ofstream f(dbfile);f<<j.dump(2);}
string tokenRole(const httplib::Request& req){auto token=req.get_header_value("Authorization");if(token=="Bearer supervisor-demo")return "supervisor";if(token=="Bearer sales-demo")return "sales";return "";}
void reply(httplib::Response& r,const json& j,int status=200){r.status=status;r.set_content(j.dump(),"application/json");}
int main(){httplib::Server app;
app.set_mount_point("/", "./public");
app.Post("/api/login",[](const httplib::Request& req,httplib::Response& res){try{auto j=json::parse(req.body);string email=j.value("email","");string password=j.value("password","");if(email=="supervisor@demo.com"&&password=="Demo123!")reply(res,{{"token","supervisor-demo"},{"role","supervisor"}});else if(email=="sales@demo.com"&&password=="Demo123!")reply(res,{{"token","sales-demo"},{"role","sales"}});else reply(res,{{"error","Invalid credentials"}},401);}catch(...){reply(res,{{"error","Invalid request"}},400);}});
app.Get("/api/data",[](const httplib::Request& req,httplib::Response& res){if(tokenRole(req).empty()){reply(res,{{"error","Unauthorized"}},401);return;}lock_guard<mutex> g(lockdb);reply(res,load());});
app.Post("/api/orders",[](const httplib::Request& req,httplib::Response& res){try{auto j=json::parse(req.body);string name=j.value("name","");string contact=j.value("contact","");string shoe=j.value("shoe","");string size=j.value("size","");string quantityType=j.value("quantityType","");int qty=j.value("quantity",0);if(name.empty()||contact.empty()||(shoe!="Shoe A"&&shoe!="Shoe B")||size.empty()||qty<1||(quantityType=="Batch"&&qty<30)){reply(res,{{"error","Check order fields; batch orders require at least 30 pairs"}},400);return;}lock_guard<mutex> g(lockdb);auto db=load();int id=1001;for(auto &o:db["orders"])id=max(id,o.value("id",0)+1);j["id"]=id;j["approved"]=false;j["phase"]=0;j["status"]="yellow";j["completed"]=0;j["dispatch"]="Pending";j["created"]="Submitted";db["orders"].push_back(j);save(db);reply(res,{{"id",id},{"message","Order submitted for review"}});}catch(...){reply(res,{{"error","Invalid order"}},400);}});
app.Post("/api/manage",[](const httplib::Request& req,httplib::Response& res){string role=tokenRole(req);if(role.empty()){reply(res,{{"error","Unauthorized"}},401);return;}try{auto j=json::parse(req.body);string action=j.value("action","");lock_guard<mutex> g(lockdb);auto db=load();if(action=="approve"||action=="progress"||action=="dispatch"){if(role!="sales"&&action!="progress"){reply(res,{{"error","Forbidden"}},403);return;}if(action=="progress"&&role!="supervisor"){reply(res,{{"error","Forbidden"}},403);return;}bool found=false;for(auto &o:db["orders"])if(o.value("id",0)==j.value("id",-1)){found=true;if(action=="approve"){o["approved"]=true;o["phase"]=1;}if(action=="progress"&&o.value("approved",false)){o["phase"]=clamp(j.value("phase",1),1,5);string s=j.value("status","yellow");if(s!="green"&&s!="yellow"&&s!="red")s="yellow";o["status"]=s;o["completed"]=clamp(j.value("completed",0),0,o.value("quantity",0));}if(action=="dispatch")o["dispatch"]=j.value("dispatch","Pending");break;}if(!found){reply(res,{{"error","Order not found"}},404);return;}}
else if(role=="supervisor"&&action=="stock"){int idx=j.value("index",-1);if(idx<0||idx>=int(db["materials"].size())){reply(res,{{"error","Invalid material"}},400);return;}db["materials"][idx]["stock"]=max(0,j.value("stock",0));}
else if(role=="supervisor"&&(action=="supply"||action=="defect")){auto record=j.value("record",json::object());db[action=="supply"?"supplies":"defects"].push_back(record);}
else{reply(res,{{"error","Forbidden or unknown action"}},403);return;}save(db);reply(res,{{"ok",true}});}catch(...){reply(res,{{"error","Invalid request"}},400);}});
cout<<"Open http://localhost:8080\n";app.listen("0.0.0.0",8080);}
