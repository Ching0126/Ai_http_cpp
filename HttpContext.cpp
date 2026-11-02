#include "HttpContext.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace{

constexpr std::size_t maxHeader = 8192;
constexpr std::size_t maxBody = 65536;

std::string lower(std::string text) {
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return text;
}
}//namespace

HttpContext::Result HttpContext::fail(
    int status,
    const std::string& reason,
    const std::string& body
){
    errorStatus_ = status;
    errorReason_ = reason;
    errorBody_ = body;

    state_ = State::Error;

    return Result::Error;
}

bool HttpContext::parseRequestLine(
    const std::string& line
){
    std::istringstream input(line);//把一行请求行放到输入流里
    std::string extra;

    return static_cast<bool>(//按照空行分隔请求行的三个部分，分别是 method、path、version
        input>>
        request_.method>>
        request_.path>>
        request_.version
    )&&
    !(input>>extra)&&//没用额外信息
    request_.version =="HTTP/1.1";//只支持 HTTP/1.1
}

bool HttpContext::parseHeader(
    const std::string& line
){
    const auto colon = line.find(':');

    if(colon==std::string::npos || colon ==0){//找不到：
        return false;
    }

    const auto name = lower(line.substr(0,colon));

    const auto value = line.substr(colon+1);

    if(name =="transfer-encoding"){
        return false;
    }

    if (name == "content-length") {
        std::istringstream input(value);

        std::string digits;
        std::string extra;

        input >> digits;

        // 拒绝重复长度、空值和非数字值。
        if (hasContentLength_ ||//已经读取过
            digits.empty() ||//空值
            (input >> extra) ||//存在额外信息
            digits.find_first_not_of("0123456789") !=//存在非数字
                std::string::npos) {
            return false;
        }

        // 逐位转换，并在超过限制时立即停止。
        for (char c : digits) {
            contentLength_ =
                contentLength_ * 10 +
                static_cast<unsigned>(c - '0');

            if (contentLength_ > maxBody) {
                fail(
                    413,
                    "Payload Too Large",
                    R"({"error":"body too large"})"
                );

                return false;
            }
        }

        hasContentLength_ = true;
    }

    request_.headers[name] = value;//解析一行请求头，并且对content-length进行特殊处理，记录正文长度和是否已经读取过content-length

    return true;
}

HttpContext::Result HttpContext::parse(//解析缓冲区中的数据,请求行和请求头的解析
    muduo::net::Buffer* buffer
){
     while (true) {
        if (state_ == State::Complete) {
            return Result::Complete;
        }

        if (state_ == State::Error) {
            return Result::Error;
        }

        if (state_ == State::Body) {
            // 正文尚未收齐时，不消费已有数据。
            if (buffer->readableBytes() < contentLength_) {
                return Result::NeedMore;
            }

            request_.body =//存放请求体
                buffer->retrieveAsString(contentLength_);

            state_ = State::Complete;

            continue;
        }

        // 请求行和请求头按 CRLF 分行。
        const char* crlf = buffer->findCRLF();//在缓冲区找\r\n

        if (!crlf) {
            if (headerBytes_ + buffer->readableBytes() >
                maxHeader) {
                return fail(
                    431,
                    "Request Header Fields Too Large",
                    R"({"error":"headers too large"})"
                );
            }

            // 当前行没有收齐，等待更多数据。
            return Result::NeedMore;
        }

        const std::size_t lineBytes =//当前行的字节数
            static_cast<std::size_t>(
                crlf - buffer->peek()
            );

        // 加上行尾的两个字节：\r\n。
        headerBytes_ += lineBytes + 2;

        if (headerBytes_ > maxHeader) {
            return fail(
                431,
                "Request Header Fields Too Large",
                R"({"error":"headers too large"})"
            );
        }

        // 复制当前行，然后从缓冲区中消费这一行。
        std::string line(
            buffer->peek(),
            lineBytes
        );

        buffer->retrieve(lineBytes + 2);//消费当前行

        if (state_ == State::RequestLine) {
            if (!parseRequestLine(line)) {//解析请求行失败
                return fail(
                    400,
                    "Bad Request",
                    R"({"error":"invalid request line"})"
                );
            }

            state_ = State::Headers;
        } else if (line.empty()) {//请求头解析结束
            // 空行表示请求头结束。
            if (request_.method == "POST" &&//没找到 Content-Length
                !hasContentLength_) {
                return fail(
                    411,
                    "Length Required",
                    R"({"error":"content length required"})"
                );
            }

            state_ = State::Body;
        } else if (!parseHeader(line)) {//解析请求头失败
            // parseHeader 可能已记录 413 错误。
            if (state_ == State::Error) {
                return Result::Error;
            }

            return fail(
                400,
                "Bad Request",
                R"({"error":"invalid or unsupported header"})"
            );
        }
    }
}
