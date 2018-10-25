#include "server_wss.hpp"
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include "/usr/local/include/mysql/mysql.h"

// Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

using WssServer = SimpleWeb::SocketServer<SimpleWeb::WSS>;

map<int, string> connected_users;

typedef struct _member
{
        int uid;
        string id;
        string pwd;
        string name;
        int status; //학생 0, 강사 1
        _member(string id, string pwd, string name, int status) : id(id), pwd(pwd), name(name), status(status){
        }
        _member(){
        }
}Member;

typedef struct _lecture
{
        int id;
        int pid;
        string name;
        string content;
        int total_progress;
        int progress;
        int total_num; //총 인원
        string teacher_id;
        string teacher_name;
        _lecture(int id, string name, string content, int total_progress, int total_num, string teacher_id, string teacher_name) :
                id(id), name(name), content(content), total_progress(total_progress), total_num(total_num), teacher_id(teacher_id), teacher_name(teacher_name){
        }
        _lecture(){
        }
}Lecture;

typedef struct _room
{
        int id;
        string name;
        int now_num = 0; //현재 강의실에 들어와 있는 학생 수
        int total_num;
        int lecture_id;
        string lecture_name;
        string teacher_id;
        string teacher_name;
        _room(int id, string name, int now_num, int lecture_id, string lecture_name, string teacher_id, string teacher_name)
                : id(id),name(name),now_num(now_num),lecture_id(lecture_id),lecture_name(lecture_name),teacher_id(teacher_id),teacher_name(teacher_name){
        }
        _room(){
        }
}Room;

const char *server = "localhost";
const char *user = "root";
const char *password = "84548090";
const char *database = "network";

MYSQL *conn;
MYSQL_RES *res;
MYSQL_ROW row;

void connect();
void disconnect();
string selectPwdById(string id);
string selectMemberNameById(string id);
int insertMember(Member member);
int insertLecture(Lecture lecture);
int insertLectureInfo(string id, int lecture_id);
vector<Lecture> selectLectureById(string id);
vector<Lecture> selectLectureByTeacherId(string id);
vector<Lecture> selectAllLectures();
int insertRoom(Room room);
Room selectRoomById(int id);
vector<Room> selectAllRooms();
int updateProgressById(int process_id, int value);
int selectProgressById(string member_id, int lecture_id);

int main() {
        connect();

        //   char id[20] = "qqqqqq";
        //   char pwd[20] = "654321";
        //   char name[20] = "nananan";
        //
        // Member member(id,pwd,name, 1);
        // insertMember(member);

        // WebSocket Secure (WSS)-server at port 8080 using 1 thread
        WssServer server("server.crt", "server.key");
        server.config.port = 2053;

        // Example 0: wie WebSocket Secure endpoint
        // Added debug messages for example use of the callbacks
        // Test with the following JavaScript:
        //   var wss=new WebSocket("wss://localhost:8080/wie");
        //   wss.onmessage=function(evt){console.log(evt.data);};
        //   wss.send("test");

        // 로그인
        auto &login = server.endpoint["^/login/?$"];

        login.on_message = [](shared_ptr<WssServer::Connection> connection, shared_ptr<WssServer::Message> message) {
                                   auto message_str = message->string();

                                   printf("RECEIVED: %s\n", message_str.c_str());

                                   std::stringstream ss;
                                   ss << message_str;
                                   boost::property_tree::ptree root;
                                   try {
                                           boost::property_tree::read_json(ss, root);
                                   }
                                   catch(std::exception & e) {
                                           cout<<"JSON parsing error.\n";
                                   }
                                   string id = root.get<string>("id", "default");
                                   string password = root.get<string>("password", "default");

                                   bool success = false;
                                   int hashVal;
                                   if(id!=""&&!password.compare(selectPwdById(id))) { // 비밀번호 일치
                                           success = true;
                                           hashVal = rand() % 10000;
                                           connected_users.insert(make_pair(hashVal, id));
                                   }else{ // 비밀번호 불일치
                                           success = false;
                                   }

                                   if(success) {
                                           message_str = "SUCCESS/" + to_string(hashVal);
                                   }else{
                                           message_str = "FAIL";
                                   }
                                   auto send_stream = make_shared<WssServer::SendStream>();
                                   *send_stream << message_str;

                                   printf("SENT: %s\n\n", message_str.c_str());

                                   // connection->send is 비동기
                                   connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
                        if(ec) { // 전송 실패 메세지
                                cout << "Server: Error sending message. " <<
                                "Error: " << ec << ", error message: " << ec.message() << endl;
                        }
                });
                           };
        login.on_open = [](shared_ptr<WssServer::Connection> connection) {
                                // cout << "Server: 웹소켓 연결 성공! " << connection.get() << endl;
                        };
        // See RFC 6455 7.4.1. for status codes
        login.on_close = [](shared_ptr<WssServer::Connection> connection, int status, const string & /*reason*/) {
                                 // cout << "Server: 웹소켓 연결 끝! " << connection.get() << " with status code " << status << endl;
                         };
        // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
        login.on_error = [](shared_ptr<WssServer::Connection> connection, const SimpleWeb::error_code &ec) {
                                 cout << "Server: Error in connection " << connection.get() << ". "
                                      << "Error: " << ec << ", error message: " << ec.message() << endl;
                         };

        //회원가입
        auto &signUp = server.endpoint["^/signUp/?$"];

        signUp.on_message = [](shared_ptr<WssServer::Connection> connection, shared_ptr<WssServer::Message> message) {
                                    auto message_str = message->string();

                                    printf("RECEIVED: %s\n", message_str.c_str());

                                    std::stringstream ss;
                                    ss << message_str;
                                    boost::property_tree::ptree root;
                                    try {
                                            boost::property_tree::read_json(ss, root);
                                    }
                                    catch(std::exception & e) {
                                            cout<<"JSON parsing error.\n";
                                    }

                                    Member member;
                                    member.id = root.get<string>("id", "default");
                                    member.pwd = root.get<string>("password", "default");
                                    member.name = root.get<string>("name", "default");
                                    member.status = stoi(root.get<string>("status", "default"));

                                    bool success = false;
                                    if(insertMember(member)==1) { // 회원가입 성공
                                            success = true;
                                    }else{ // 회원가입 실패
                                            success = false;
                                    }
                                    if(success) {
                                            message_str = "SUCCESS";
                                    }else{
                                            message_str = "FAIL";
                                    }
                                    auto send_stream = make_shared<WssServer::SendStream>();
                                    *send_stream << message_str;

                                    printf("SENT: %s\n\n", message_str.c_str());
                                    // connection->send is 비동기
                                    connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
                        if(ec) { // 전송 실패 메세지
                                cout << "Server: Error sending message. " <<
                                "Error: " << ec << ", error message: " << ec.message() << endl;
                        }
                });
                            };
        signUp.on_open = [](shared_ptr<WssServer::Connection> connection) {
                                 // cout << "Server: 웹소켓 연결 성공! " << connection.get() << endl;
                         };
        signUp.on_close = [](shared_ptr<WssServer::Connection> connection, int status, const string & /*reason*/) {
                                  // cout << "Server: 웹소켓 연결 끝! " << connection.get() << " with status code " << status << endl;
                          };
        signUp.on_error = [](shared_ptr<WssServer::Connection> connection, const SimpleWeb::error_code &ec) {
                                  cout << "Server: Error in connection " << connection.get() << ". "
                                       << "Error: " << ec << ", error message: " << ec.message() << endl;
                          };

        // 강의실 목록 조회

        auto &checkClassList = server.endpoint["^/CheckClassList/?$"];

        checkClassList.on_message = [](shared_ptr<WssServer::Connection> connection, shared_ptr<WssServer::Message> message) {
                                            auto message_str = message->string();

                                            printf("RECEIVED: %s\n", message_str.c_str());
                                            //if(!message_str.compare("CheckClassList")) {// 강의실 목록 조회 요청이라면
                                            vector<Room> rooms = selectAllRooms(); // DB로부터 얻어온 강의실 목록
                                            // rooms.push_back(Room(1,"2",3,"4",5));
                                            // rooms.push_back(Room(6,"7",8,"9",10));
                                            std::stringstream ss;
                                            boost::property_tree::ptree root, Json_rooms;
                                            string id = connected_users.find(stoi(message_str))->second;

                                            for(auto room : rooms) {
                                                    boost::property_tree::ptree Json_room;
                                                    Json_room.put("id", room.id);
                                                    Json_room.put("name", room.name);

                                                    int lecture_id = room.lecture_id;
                                                    int progress = selectProgressById(id, lecture_id);

                                                    Json_room.put("lecture_progress", progress);
                                                    Json_room.put("lecture_id", lecture_id);
                                                    Json_room.put("lecture_name", room.lecture_name);
                                                    Json_room.put("teacher_id", room.teacher_id);
                                                    Json_room.put("teacher_name", room.teacher_name);
                                                    Json_room.put("now_num", room.now_num);
                                                    Json_room.put("total_num", room.total_num);
                                                    Json_rooms.push_back(make_pair("",Json_room));
                                            }
                                            root.add_child("rooms", Json_rooms);

                                            string name = selectMemberNameById(id);

                                            boost::property_tree::ptree Json_mem;
                                            Json_mem.put("mem_name", name);
                                            root.add_child("mem_data", Json_mem);
                                            try {
                                                    boost::property_tree::write_json(ss, root);
                                            }
                                            catch(std::exception & e) {
                                                    cout<<"JSON parsing error.\n";
                                            }
                                            message_str = ss.str();
                                            printf("SENT: %s\n\n", message_str.c_str());
                                            auto send_stream = make_shared<WssServer::SendStream>();
                                            *send_stream << message_str;
                                            // connection->send is 비동기
                                            connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
                        if(ec) { // 전송 실패 메세지
                                cout << "Server: Error sending message. " <<
                                "Error: " << ec << ", error message: " << ec.message() << endl;
                        }
                });
                                            //}
                                    };

        checkClassList.on_open = [](shared_ptr<WssServer::Connection> connection) {
                                         // cout << "Server: 웹소켓 연결 성공! " << connection.get() << endl;
                                 };
        checkClassList.on_close = [](shared_ptr<WssServer::Connection> connection, int status, const string & /*reason*/) {
                                          // cout << "Server: 웹소켓 연결 끝! " << connection.get() << " with status code " << status << endl;
                                  };
        checkClassList.on_error = [](shared_ptr<WssServer::Connection> connection, const SimpleWeb::error_code &ec) {
                                          cout << "Server: Error in connection " << connection.get() << ". "
                                               << "Error: " << ec << ", error message: " << ec.message() << endl;
                                  };

        // 강의실 등록시 강의 조회, 등록
        auto &registerRoom = server.endpoint["^/RegisterRoom/?$"];

        registerRoom.on_message = [](shared_ptr<WssServer::Connection> connection, shared_ptr<WssServer::Message> message) {
                                          auto message_str = message->string();

                                          std::stringstream ss, ws;
                                          ss << message_str;
                                          boost::property_tree::ptree root;
                                          printf("RECEIVED: %s\n", message_str.c_str());
                                          try {
                                                  boost::property_tree::read_json(ss, root);
                                          }
                                          catch(std::exception & e) {
                                                  cout<<"JSON parsing error.\n";
                                          }

                                          string type = root.get<string>("type", "default");
                                          string mem_id;
                                          int id;
                                          string name;

                                          if(type == "select") {
                                                  mem_id = connected_users.find(stoi(root.get<string>("hash", "default")))->second;
                                                  cout << mem_id << endl;
                                          }
                                          else{
                                                  id = stoi(root.get<string>("id", "default"));
                                                  name = root.get<string>("name", "default");
                                          }

                                          if(type == "select")
                                          {
                                                  vector<Lecture> lectures = selectLectureByTeacherId(mem_id);
                                                  boost::property_tree::ptree Json_lectures;

                                                  cout << lectures.size() << endl;
                                                  for(auto lecture : lectures) {
                                                          cout << "here" << endl;
                                                          boost::property_tree::ptree Json_lecture;
                                                          Json_lecture.put("id", lecture.id);
                                                          Json_lecture.put("name", lecture.name);
                                                          Json_lectures.push_back(make_pair("",Json_lecture));
                                                  }
                                                  try {
                                                          boost::property_tree::ptree write_root;
                                                          write_root.add_child("lectures", Json_lectures);
                                                          boost::property_tree::write_json(ws, write_root);
                                                          message_str = ws.str();
                                                  }
                                                  catch(std::exception & e) {
                                                          cout<<"JSON parsing error.\n";
                                                  }
                                          }
                                          else
                                          {
                                                  Room room;
                                                  room.lecture_id = id;
                                                  room.name = name;

                                                  if(insertRoom(room) == 1)
                                                          message_str = "SUCCESS";
                                                  else
                                                          message_str = "FAIL";
                                          }
                                          auto send_stream = make_shared<WssServer::SendStream>();
                                          *send_stream << message_str;
                                          printf("SENT: %s\n\n", message_str.c_str());
                                          // connection->send is 비동기
                                          connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
                        if(ec) { // 전송 실패 메세지
                                cout << "Server: Error sending message. " <<
                                "Error: " << ec << ", error message: " << ec.message() << endl;
                        }
                });
                                  };
        registerRoom.on_open = [](shared_ptr<WssServer::Connection> connection) {
                                       // cout << "Server: 웹소켓 연결 성공! " << connection.get() << endl;
                               };
        registerRoom.on_close = [](shared_ptr<WssServer::Connection> connection, int status, const string & /*reason*/) {
                                        // cout << "Server: 웹소켓 연결 끝! " << connection.get() << " with status code " << status << endl;
                                };
        registerRoom.on_error = [](shared_ptr<WssServer::Connection> connection, const SimpleWeb::error_code &ec) {
                                        cout << "Server: Error in connection " << connection.get() << ". "
                                             << "Error: " << ec << ", error message: " << ec.message() << endl;
                                };

        // 강의 목록 조회, 수강신청
        auto &checkLectureList = server.endpoint["^/CheckLectureList/?$"];

        checkLectureList.on_message = [](shared_ptr<WssServer::Connection> connection, shared_ptr<WssServer::Message> message) {
                                              auto message_str = message->string();

                                              std::stringstream ss;
                                              ss << message_str;
                                              printf("RECEIVED: %s\n", message_str.c_str());
                                              boost::property_tree::ptree root;

                                              try {
                                                      boost::property_tree::read_json(ss, root);
                                              }
                                              catch(std::exception & e) {
                                                      cout<<"JSON parsing error.\n";
                                              }

                                              string mem_id = connected_users.find(stoi(root.get<string>("hash", "default")))->second;
                                              int lecture_id = stoi(root.get<string>("lecture_id", "default"));

                                              if(insertLectureInfo(mem_id, lecture_id) == 1)
                                                      message_str = "SUCCESS";
                                              else
                                                      message_str = "FAIL";

                                              auto send_stream = make_shared<WssServer::SendStream>();
                                              *send_stream << message_str;
                                              printf("SENT: %s\n\n", message_str.c_str());
                                              // connection->send is 비동기
                                              connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
                        if(ec) { // 전송 실패 메세지
                                cout << "Server: Error sending message. " <<
                                "Error: " << ec << ", error message: " << ec.message() << endl;
                        }
                });

                                      };
        checkLectureList.on_open = [](shared_ptr<WssServer::Connection> connection) {
                                           vector<Lecture> lectures = selectAllLectures();

                                           std::stringstream ss;
                                           boost::property_tree::ptree root, Json_lectures;
                                           for(auto lecture : lectures) {
                                                   boost::property_tree::ptree Json_lecture;
                                                   Json_lecture.put("id", lecture.id);
                                                   Json_lecture.put("name", lecture.name);
                                                   Json_lecture.put("content", lecture.content);
                                                   Json_lecture.put("total_progress", lecture.total_progress);
                                                   Json_lecture.put("total_num", lecture.total_num);
                                                   Json_lecture.put("teacher_name", lecture.teacher_name);
                                                   Json_lectures.push_back(make_pair("",Json_lecture));
                                           }
                                           root.add_child("lectures", Json_lectures);
                                           try {
                                                   boost::property_tree::write_json(ss, root);
                                           }
                                           catch(std::exception & e) {
                                                   cout<<"JSON parsing error.\n";
                                           }
                                           string message_str = ss.str();

                                           auto send_stream = make_shared<WssServer::SendStream>();
                                           *send_stream << message_str;
                                           // connection->send is 비동기
                                           printf("SENT: %s\n\n", message_str.c_str());
                                           connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
                        if(ec) { // 전송 실패 메세지
                                cout << "Server: Error sending message. " <<
                                "Error: " << ec << ", error message: " << ec.message() << endl;
                        }
                });
                                           //}
                                   };
        checkLectureList.on_close = [](shared_ptr<WssServer::Connection> connection, int status, const string & /*reason*/) {
                                            // cout << "Server: 웹소켓 연결 끝! " << connection.get() << " with status code " << status << endl;
                                    };
        checkLectureList.on_error = [](shared_ptr<WssServer::Connection> connection, const SimpleWeb::error_code &ec) {
                                            cout << "Server: Error in connection " << connection.get() << ". "
                                                 << "Error: " << ec << ", error message: " << ec.message() << endl;
                                    };

        // 내 강의 목록 조회
        auto &myLectureList = server.endpoint["^/MyLectureList/?$"];

        myLectureList.on_message = [](shared_ptr<WssServer::Connection> connection, shared_ptr<WssServer::Message> message) {
                                           auto message_str = message->string();
                                           std::stringstream ss, ws;
                                           ss << message_str;
                                           printf("RECEIVED: %s\n", message_str.c_str());
                                           boost::property_tree::ptree read_root;
                                           try {
                                                   boost::property_tree::read_json(ss, read_root);
                                           }
                                           catch(std::exception & e) {
                                                   cout<<"JSON parsing error.\n";
                                           }

                                           string type = read_root.get<string>("type", "default"); // type
                                           string mem_id;
                                           int id;
                                           int changeVal;

                                           if(type == "select") { // select
                                                   mem_id = connected_users.find(stoi(read_root.get<string>("hash", "default")))->second;
                                           }
                                           else{ // update
                                                   id = stoi(read_root.get<string>("id", "default"));
                                                   changeVal = stoi(read_root.get<string>("changeVal", "default"));
                                           }

                                           if(type == "select")
                                           {
                                                   vector<Lecture> lectures = selectLectureById(mem_id);

                                                   boost::property_tree::ptree Json_lectures;

                                                   for(auto lecture : lectures)
                                                   {
                                                           boost::property_tree::ptree Json_lecture;
                                                           Json_lecture.put("id", lecture.id);
                                                           Json_lecture.put("pid", lecture.pid);
                                                           Json_lecture.put("name", lecture.name);
                                                           Json_lecture.put("content", lecture.content);
                                                           Json_lecture.put("total_progress", lecture.total_progress);
                                                           Json_lecture.put("progress", lecture.progress);
                                                           Json_lecture.put("teacher_name", lecture.teacher_name);
                                                           Json_lectures.push_back(make_pair("",Json_lecture));
                                                   }

                                                   try {
                                                           boost::property_tree::ptree write_root;
                                                           write_root.add_child("lectures", Json_lectures);
                                                           boost::property_tree::write_json(ws, write_root);
                                                           message_str = ws.str();
                                                   }
                                                   catch(std::exception & e) {
                                                           cout<<"JSON parsing error.\n";
                                                   }
                                           }
                                           else
                                           {
                                                   if(updateProgressById(id, changeVal) == 1)
                                                           message_str = "SUCCESS";
                                                   else
                                                           message_str = "FAIL";
                                           }
                                           auto send_stream = make_shared<WssServer::SendStream>();
                                           *send_stream << message_str;
                                           printf("SENT: %s\n\n", message_str.c_str());
                                           // connection->send is 비동기
                                           connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
                        if(ec) { // 전송 실패 메세지
                                cout << "Server: Error sending message. " <<
                                "Error: " << ec << ", error message: " << ec.message() << endl;
                        }
                });
                                   };
        myLectureList.on_open = [](shared_ptr<WssServer::Connection> connection) {
                                        // cout << "Server: 웹소켓 연결 성공! " << connection.get() << endl;
                                };
        myLectureList.on_close = [](shared_ptr<WssServer::Connection> connection, int status, const string & /*reason*/) {
                                         // cout << "Server: 웹소켓 연결 끝! " << connection.get() << " with status code " << status << endl;
                                 };
        myLectureList.on_error = [](shared_ptr<WssServer::Connection> connection, const SimpleWeb::error_code &ec) {
                                         cout << "Server: Error in connection " << connection.get() << ". "
                                              << "Error: " << ec << ", error message: " << ec.message() << endl;
                                 };

        // 강의 등록
        auto &registerLecture = server.endpoint["^/RegisterLecture/?$"];
        registerLecture.on_message = [](shared_ptr<WssServer::Connection> connection, shared_ptr<WssServer::Message> message) {
                                             auto message_str = message->string();
                                             printf("RECEIVED: %s\n", message_str.c_str());
                                             std::stringstream ss;
                                             ss << message_str;
                                             boost::property_tree::ptree root;
                                             try {
                                                     boost::property_tree::read_json(ss, root);
                                             }
                                             catch(std::exception & e) {
                                                     cout<<"JSON parsing error.\n";
                                             }

                                             Lecture lecture;
                                             lecture.name = root.get<string>("name", "default");
                                             lecture.total_num = stoi(root.get<string>("total_num", "default"));
                                             lecture.total_progress = stoi(root.get<string>("total_progress", "default"));
                                             lecture.content = root.get<string>("content", "default");
                                             lecture.teacher_id = connected_users.find(stoi(root.get<string>("hash", "default")))->second;

                                             bool success = false;
                                             if(insertLecture(lecture)==1) { // 강의등록 성공
                                                     success = true;
                                             }else{ // 강의등록 실패
                                                     success = false;
                                             }
                                             if(success) {
                                                     message_str = "SUCCESS";
                                             }else{
                                                     message_str = "FAIL";
                                             }
                                             auto send_stream = make_shared<WssServer::SendStream>();
                                             *send_stream << message_str;
                                             printf("SENT: %s\n\n", message_str.c_str());
                                             // connection->send is 비동기
                                             connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
                        if(ec) { // 전송 실패 메세지
                                cout << "Server: Error sending message. " <<
                                "Error: " << ec << ", error message: " << ec.message() << endl;
                        }
                });
                                     };
        registerLecture.on_open = [](shared_ptr<WssServer::Connection> connection) {
                                          // cout << "Server: 웹소켓 연결 성공! " << connection.get() << endl;
                                  };
        registerLecture.on_close = [](shared_ptr<WssServer::Connection> connection, int status, const string & /*reason*/) {
                                           // cout << "Server: 웹소켓 연결 끝! " << connection.get() << " with status code " << status << endl;
                                   };
        registerLecture.on_error = [](shared_ptr<WssServer::Connection> connection, const SimpleWeb::error_code &ec) {
                                           cout << "Server: Error in connection " << connection.get() << ". "
                                                << "Error: " << ec << ", error message: " << ec.message() << endl;
                                   };

        // Example 3: Echo to all WebSocket Secure endpoints
        // Sending received messages to all connected clients
        // Test with the following JavaScript on more than one browser windows:
        //   var wss=new WebSocket("wss://localhost:8080/echo_all");
        //   wss.onmessage=function(evt){console.log(evt.data);};
        //   wss.send("test");

        auto &echo_all = server.endpoint["^/echo_all/?$"];
        echo_all.on_message = [&server](shared_ptr<WssServer::Connection> /*connection*/, shared_ptr<WssServer::Message> message) {
                                      auto send_stream = make_shared<WssServer::SendStream>();
                                      *send_stream << message->string();

                                      // echo_all.get_connections() can also be used to solely receive connections on this endpoint
                                      for(auto &a_connection : server.get_connections())
                                              a_connection->send(send_stream);
                              };
        echo_all.on_open = [](shared_ptr<WssServer::Connection> connection) {
                                   cout << "Server: Opened connection " << connection.get() << endl;
                           };

        // See RFC 6455 7.4.1. for status codes
        echo_all.on_close = [](shared_ptr<WssServer::Connection> connection, int status, const string & /*reason*/) {
                                    cout << "Server: Closed connection " << connection.get() << " with status code " << status << endl;
                            };

        // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
        echo_all.on_error = [](shared_ptr<WssServer::Connection> connection, const SimpleWeb::error_code &ec) {
                                    cout << "Server: Error in connection " << connection.get() << ". "
                                         << "Error: " << ec << ", error message: " << ec.message() << endl;
                            };

        thread server_thread([&server]() {
                             // Start WSS-server
                             server.start();
                });

        server_thread.join();

        disconnect();
}

/**
   DB 연결
 */
void connect()
{
        conn = mysql_init(NULL);
        //mysql 서버로 접속 시도
        if(!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0))
        {
                printf("%s\n", mysql_error(conn));
                exit(1);
        }
}

/**
   DB 연결 끊음
 */
void disconnect()
{
        mysql_close(conn);
}


/**
   로그인
   parameter: id
   return: pwd
 */
string selectPwdById(string id)
{
        string pwd;
        char query[1000];

        sprintf(query, "SELECT pwd FROM member WHERE id = '%s'", id.c_str());

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return pwd;
        }

        res = mysql_store_result(conn);
        if((row = mysql_fetch_row(res)) != NULL)
                pwd = row[0];

        mysql_free_result(res);
        return pwd;
}

/**
   회원 이름 조회
   parameter: id
   return: name
 */
string selectMemberNameById(string id)
{
        string name;
        char query[1000];

        sprintf(query, "SELECT name FROM member WHERE id = '%s'", id.c_str());

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return name;
        }

        res = mysql_store_result(conn);
        if((row = mysql_fetch_row(res)) != NULL)
                name = row[0];

        mysql_free_result(res);
        return name;
}

/**
   회원 가입
   parameter: Member struct
   return: 1(success) / 0(fail)
 */
int insertMember(Member member)
{
        char query[1000];
        sprintf(query, "INSERT INTO member VALUES('%s', '%s', '%s', %d)", member.id.c_str(), member.pwd.c_str(), member.name.c_str(), 1);

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return 0;
        }
        return 1;
}

/**
   (강사) 강의 등록
   parameter: Lecture struct
   return: 1(success) / 0(fail)
 */
int insertLecture(Lecture lecture)
{
        char query[1000];
        sprintf(query, "INSERT INTO lecture(name, content, total_progress, total_num, teacher_id) VALUES('%s', '%s', %d, %d, '%s')",
                lecture.name.c_str(), lecture.content.c_str(), lecture.total_progress, lecture.total_num, lecture.teacher_id.c_str());

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return 0;
        }
        return 1;
}

/**
   (학생) 강의 신청
   parameter: member id, lecture id
   return: 1(success ) / 0(fail)
 */
int insertLectureInfo(string member_id, int lecture_id)
{
        char query[1000];
        sprintf(query, "INSERT INTO lecture_info(member_id, lecture_id, progress) VALUES('%s', %d, 0)", member_id.c_str(), lecture_id);

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return 0;
        }
        return 1;
}

/**
   내 강의 조회 - 학생
   parameter: member id
   return: Lecture vector
 */
vector<Lecture> selectLectureById(string mem_id)
{
        vector<Lecture> vec;
        char query[1000];

        sprintf(query, "SELECT l.id, l.name, l.content, m.name, l.total_progress, i.progress, i.id FROM lecture l, lecture_info i, member m WHERE i.member_id = '%s' AND l.id = i.lecture_id AND l.teacher_id = m.id", mem_id.c_str());

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return vec;
        }

        res = mysql_use_result(conn);
        while((row = mysql_fetch_row(res)) != NULL)
        {
                Lecture lecture;
                lecture.id = stoi(row[0]);
                lecture.name = row[1];
                lecture.content = row[2];
                lecture.teacher_name = row[3];
                lecture.total_progress = stoi(row[4]);
                lecture.progress = stoi(row[5]);
                lecture.pid = stoi(row[6]);
                vec.push_back(lecture);
        }
        mysql_free_result(res);
        return vec;
}
/**
   내 강의 조회 - 강사
   parameter: member id
   return: Lecture vector
 */
vector<Lecture> selectLectureByTeacherId(string teacher_id)
{
        vector<Lecture> vec;
        char query[1000];

        cout << "db: " + teacher_id << endl;
        sprintf(query, "SELECT id, name, content FROM lecture WHERE teacher_id = '%s'", teacher_id.c_str());

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return vec;
        }

        res = mysql_use_result(conn);
        while((row = mysql_fetch_row(res)) != NULL)
        {
                Lecture lecture;
                lecture.id = stoi(row[0]);
                lecture.name = row[1];
                lecture.content = row[2];
                vec.push_back(lecture);
        }
        mysql_free_result(res);
        return vec;
}

/**
   모든 강의 목록 조회
   parmeter: X
   return: Lecture vector
 */
vector<Lecture> selectAllLectures()
{
        vector<Lecture> vector;
        char query[1000];

        sprintf(query, "SELECT l.id, l.name, l.content, l.total_progress, l.total_num, m.name FROM lecture l, member m WHERE l.teacher_id = m.id");

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return vector;
        }
        res = mysql_use_result(conn);
        while((row = mysql_fetch_row(res)) != NULL)
        {
                Lecture lecture;
                lecture.id = stoi(row[0]);
                lecture.name = row[1];
                lecture.content = row[2];
                lecture.total_progress = stoi(row[3]);
                lecture.total_num = stoi(row[4]);
                lecture.teacher_name = row[5];
                vector.push_back(lecture);
        }
        mysql_free_result(res);
        return vector;
}

/**
   강의실 등록
   parameter: Room struct
   return: 1(success) / 0(fail)
 */
int insertRoom(Room room)
{
        char query[1000];
        sprintf(query, "INSERT INTO lecture_room(name, lecture_id) VALUES('%s', %d)", room.name.c_str(), room.lecture_id);

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return 0;
        }
        return 1;
}

/**
   하나의 강의실 조회
   parameter: Room id
   return: Room struct
 **/
Room selectRoomById(int room_id)
{
        Room room;
        char query[1000];

        sprintf(query, "SELECT * FROM lecture_room WHERE id = %d", room_id);

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return room;
        }
        res = mysql_store_result(conn);
        if((row = mysql_fetch_row(res)) != NULL)
        {
                room.id = room_id;
                room.name = row[1];
        }
        mysql_free_result(res);
        return room;
}

/**
   하나의 강의실 조회
   parameter: Room id
   return: Room struct
 **
   Room selectRoomById(int member_id)
   {
   Room room;
   char query[1000];

   sprintf(query, "SELECT * FROM lecture_room WHERE mem_id = %d", room_id);

   if(mysql_query(conn, query))
   {
   cout << mysql_error(conn) << endl;
   return room;
   }
   res = mysql_store_result(conn);
   if((row = mysql_fetch_row(res)) != NULL)
   {
   room.id = room_id;
   room.name = row[1];
   }
   mysql_free_result(res);
   return room;
   }
 */

/**
   모든 강의실 목록 조회
   parameter: X
   return: Room list
 */
vector<Room> selectAllRooms()
{
        vector<Room> vector;
        char query[1000];

        // sprintf(query, "SELECT r.id, r.name, l.teacher_id, m.name, l.id, l.name, l.total_num FROM lecture_room r, lecture l, member m WHERE r.lecture_id = l.id AND l.teacher_id = m.id");
        sprintf(query, "SELECT r.id, r.name, l.teacher_id, m.name, l.id, l.name, l.total_num "
                "FROM lecture_room r, lecture l, member m "
                "WHERE r.lecture_id = l.id AND l.teacher_id = m.id");

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return vector;
        }
        res = mysql_use_result(conn);
        while((row = mysql_fetch_row(res)) != NULL)
        {
                Room room;
                room.id = stoi(row[0]);
                room.name = row[1];
                room.teacher_id = row[2];
                room.teacher_name = row[3];
                room.lecture_id = stoi(row[4]);
                room.lecture_name = row[5];
                room.total_num = stoi(row[6]);
                vector.push_back(room);
        }
        mysql_free_result(res);
        return vector;
}

/**
   학생 진도 업데이트
   parameter: process id, change value
   return: 1(success) / 0(fail)
 **/
int updateProgressById(int process_id, int value)
{
        char query[1000];

        sprintf(query, "UPDATE lecture_info SET progress = %d WHERE id = %d", value, process_id);

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return 0;
        }
        return 1;
}

/**
   어떤 강의에 해당하는 한 학생의 진도 조회
   parameter: member id, lecture id
   return: progress
 */
int selectProgressById(string member_id, int lecture_id)
{
        int progress = 0;
        char query[1000];

        sprintf(query, "SELECT i.progress FROM lecture l, member m, lecture_info i "
                "WHERE m.id = '%s' AND l.id = %d AND l.id = i.lecture_id AND m.id = i.member_id", member_id.c_str(), lecture_id);

        if(mysql_query(conn, query))
        {
                cout << mysql_error(conn) << endl;
                return progress;
        }
        res = mysql_store_result(conn);
        if((row = mysql_fetch_row(res)) != NULL)
        {
                progress = stoi(row[0]);
        }
        mysql_free_result(res);
        return progress;
}
/*
   ptree pt;
   ptree children;
   ptree child1, child2, child3;

   child1.put("childkeyA", 1);
   child1.put("childkeyB", 2);

   child2.put("childkeyA", 3);
   child2.put("childkeyB", 4);

   child3.put("childkeyA", 5);
   child3.put("childkeyB", 6);

   children.push_back(std::make_pair("", child1));
   children.push_back(std::make_pair("", child2));
   children.push_back(std::make_pair("", child3));

   pt.put("testkey", "testvalue");
   pt.add_child("MyArray", children);

   write_json("test2.json", pt);

   {
   "testkey": "testvalue",
   "MyArray":
   [
   {
   "childkeyA": "1",
   "childkeyB": "2"
   },
   {
   "childkeyA": "3",
   "childkeyB": "4"
   },
   {
   "childkeyA": "5",
   "childkeyB": "6"
   }
   ]
   }
 */
