#include "ChatServer.h"

#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>

#include <boost/json/src.hpp>

#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>
#include <string_view>

namespace{

constexpr std::size_t maxHeader = 8192;
constexpr std::size_t maxBody = 65536;

std::string lower(std::string text){//字符串转化为小写
    std::transform(
        text.begin(),//从哪开始读
        text.end(),//读到哪里结束
        text.begin(),//从哪里开始写
        [](unsigned char c){//对每个字符执行的操作
            return static_cast<char>(std::tolower(c));//返回的是int 需要进行类型转换
        }
    );
    return text;
}

}//namespace

ChatServer::ChatServer(
    muduo::net::EventLoop* loop,
    unsigned short port
)
    :server_(
        loop,
        muduo::net::InetAddress("127.0.0.1",port),
        "AiHttp"
    ){
    //将Muduo的数据到达时间绑定成员函数
    server_.setMessageCallback(
        std::bind(
            &ChatServer::onMessage,//这个函数的三个参数
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        )
    );
}

void ChatServer::start(){
    server_.start();
}
//把处理结果拼成一条 HTTP 响应，发给客户端，然后关闭连接。
void ChatServer::reply(
    const muduo::net::TcpConnectionPtr& conn,
    int status,//状态码
    const std::string& reason,//状态码对于中文描述
    const std::string& body//json字符串
){
    //http状态行 HTTP/1.1 200 OK
    std::string response = 
        "HTTP/1.1 " +
        std::to_string(status)+
        " "+
        reason +
        "\r\n";
    
    //http响应头
    response +=
        "Content-Type:application/json;charset=utf-8\r\n";

    response +=
        "Content-Length:"+
        std::to_string(body.size())+
        "\r\n";
    
    response += "Connection:close\r\n";

    response +="\r\n";//空行用于分割响应头和正文。

    response += body;

    conn->send(response);

    conn->shutdown();

    /* HTTP/1.1 200 OK
    Content-Type:application/json;charset=utf-8
    Content-Length:19
    Connection:close
    {"answer":"你好"} */
}
//收到客户端发来的数据后，检查它是不是一个完整、合法的 HTTP 请求。
/* 
收到数据
   ↓
找到请求头，并检查请求头
   ↓
确定正文长度，等待完整正文
   ↓
检查请求方法和路径
   ↓
把正文交给 handleChat
*/
void ChatServer::onMessage(
    const muduo::net::TcpConnectionPtr& conn,
    muduo::net::Buffer* buffer,
    muduo::Timestamp
){
    if(!conn->connected()){//检查连接
        return ;
    }

    std::string_view data(
        buffer->peek(),
        buffer->readableBytes()
    );
    //请求头和请求行之间的空行
    const auto headerEnd = data.find("\r\n\r\n");
    //没找到请求头末尾
    if(headerEnd==std::string_view::npos){
        if(data.size()>maxHeader){//数据太大，超出范围
            reply(
                conn,
                431,
                "Request Header Fields Too Large",
                R"({"error":"headers too large"})"
            );
        }
        return;//还没接收完整
    }

    if(headerEnd +4 >maxHeader){//完整请求头包括\n\r\n\r,这是四个字节的数据
        reply(
            conn,
            431,
            "Request Header Fields Too Large",
                R"({"error":"headers too large"})"
        );
        return ;
    }

    // POST /chat/send HTTP/1.1 请求行 method >> path >> version
    // Content-Type: application/json
    // Content-Length: 20
    
    // {"question":"你好"}

    std::istringstream headers(//从收到的数据中取出 HTTP 请求头，放进一个输入流，方便后面一行一行地读取。
      std::string(data.substr(0,headerEnd))  
    );

    std::string line;//以后数据
    std::string method;//POST
    std::string path;//chat/send
    std::string version;//HTTP/1.1
    std::string extra;

    std::getline(headers,line);//POST /chat/send HTTP/1.1

    std::istringstream requestLine(line);//一行数据变成输入流
    if(!(requestLine>>method>>path>>version)||//按照空白字符分割，一次放进三个变量中
        (requestLine>>extra)||
        version != "HTTP/1.1"){
        reply(
            conn,
            400,
            "Bad Request",
            R"({"error":"invalid request line"})"
        );
        return;
    }

    std::size_t length = 0;
    bool hasLength = false;
    // POST /chat/send HTTP/1.1 请求行 method >> path >> version
    // Content-Type: application/json
    // Content-Length: 20
    
    // {"question":"你好"}
    while(std::getline(headers,line)){
        if(!line.empty()&&line.back()=='\r'){
            line.pop_back();//删去最后一个字符
        }

        const auto colon = line.find(':');//拆除请求头名称

        if(colon == std::string::npos){
            reply(
            conn,
            400,
            "Bad Request",
            R"({"error":"invalid header"})"
            );
            return ;
        }

        const auto name = lower(line.substr(0,colon));//请求头名称

        if(name=="transfer-encoding"){//特殊的请求头，不支持
            reply(
            conn,
            400,
            "Bad Request",
            R"({"error":"transfer encoding unsupported"})"
            );
            return;
        }

        if(name!="content-length"){//只有读到 Content-Length才继续往下
            continue;
        }

        std::istringstream value(line.substr(colon+1));//长度值

        std::string digits;
        value>>digits;//读取值,可以跳过空白

        if(hasLength||//是否重复？
        digits.empty()||//受否为空
        (value>>extra)||//是的存在多余内容？
        digits.find_first_not_of("0123456789")!=//是否存在非数字内容
            std::string::npos){
            reply(
            conn,
            400,
            "Bad Request",
            R"({"error":"invalid content length"})"
            );
            return;
        }
        //把字符串转成实际长度
        for(char c:digits){
            length =
                length*10+
                static_cast<unsigned>(c-'0');
            
            if(length>maxBody){//正文太大，报错
                reply(
                conn,
                413,
                "Payload Too Large",
                R"({"error":"body too large"})"
                );
                return;
            }
        }

        hasLength = true;//首次获取长度后赋值为true
    }

    if(method =="POST"&& !hasLength){//http请求没有content-length
        reply(
            conn,
            411,
            "Length Required",
            R"({"error":"content length required"})"
        );
        return;
    }
    //总长度
    const auto total = headerEnd + 4 + length;

    if(data.size()<total){
        return;
    }

    std::string body(//获取正文
      data.substr(headerEnd+4,length)  
    );

    buffer->retrieve(total);//缓冲区中移除

    if(method != "POST" || path != "/chat/send"){//检查路由
         reply(
            conn,
            404,
            "Not Found",
            R"({"error":"route not found"})"
        );
        return;
    }
    handleChat(conn,body);
}
//处理聊天请求的正文
void ChatServer::handleChat(
    const muduo::net::TcpConnectionPtr& conn,
    const std::string& body
){
    //把正文解析成json
    boost::system::error_code ec;//记录是否出错

    const auto request = boost::json::parse(body,ec);

    if(ec||!request.is_object()){//解析失败或者不是合法对象
        reply(
            conn,
            400,
            "Bad Request",
            R"({"error":"body must be a JSON object"})"
        );
        return;
    }
    //查找question字段
    const auto* question=
        request.as_object().if_contains("question");

    if(!question ||//有无question字段
       !question->is_string()||//字段值是不是字符串
       question->as_string().empty()){//字符串是否为空？
         reply(
            conn,
            400,
            "Bad Request",
            R"({"error":"question must be a non-empty string"})"
        );
        return;
    }

    const std::string text(//取出json中的字符串
      question->as_string().c_str(),
      question->as_string().size()  
    );

    boost::json::object response;

    response["success"] = true;
    response["answer"]=
        "[模拟AI]收到你的信息："+text;
    
    reply(
        conn,
        200,
        "OK",
        boost::json::serialize(response)
    );
}