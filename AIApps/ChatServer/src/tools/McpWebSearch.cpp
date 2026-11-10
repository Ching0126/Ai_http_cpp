#include "tools/McpWebSearch.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

namespace json = boost::json;

// 单次 MCP 响应最多接收 1 MB。
constexpr std::size_t maxBytes = 1024 * 1024;

// 保存一次 HTTP 请求收到的正文和 MCP 会话 ID。
struct Response {
    std::string body;
    std::string session;
};

// 接收 HTTP 响应正文，并限制大小。
//
// 不能让 C++ 异常穿过 libcurl 的 C 回调，
// 因此追加失败时返回 0。
std::size_t writeBody(
    char* data,
    std::size_t size,
    std::size_t count,
    void* ptr
) {
    if (size != 0 && count > maxBytes / size) {
        return 0;
    }

    const auto bytes = size * count;
    auto& body = static_cast<Response*>(ptr)->body;

    if (body.size() > maxBytes - bytes) {
        return 0;
    }

    try {
        body.append(data, bytes);
    } catch (...) {
        return 0;
    }

    return bytes;
}

// 读取响应头中的 Mcp-Session-Id。
//
// 初始化后，后续请求需要携带该会话 ID。
std::size_t readHeader(
    char* data,
    std::size_t size,
    std::size_t count,
    void* ptr
) {
    if (size != 0 && count > maxBytes / size) {
        return 0;
    }

    const auto bytes = size * count;

    try {
        std::string line(data, bytes);

        const auto colon = line.find(':');

        if (colon == std::string::npos) {
            return bytes;
        }

        std::string name = line.substr(0, colon);

        // HTTP 请求头名称不区分大小写，统一转成小写。
        std::transform(
            name.begin(),
            name.end(),
            name.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        if (name == "mcp-session-id") {
            std::string value = line.substr(colon + 1);

            // 去掉值前后的空白字符。
            while (
                !value.empty() &&
                std::isspace(
                    static_cast<unsigned char>(value.front())
                )
            ) {
                value.erase(value.begin());
            }

            while (
                !value.empty() &&
                std::isspace(
                    static_cast<unsigned char>(value.back())
                )
            ) {
                value.pop_back();
            }

            // 限制长度和字符范围，避免把非法值放进请求头。
            if (
                value.empty() ||
                value.size() > 256 ||
                !std::all_of(
                    value.begin(),
                    value.end(),
                    [](unsigned char c) {
                        return c >= 0x21 && c <= 0x7e;
                    }
                )
            ) {
                return 0;
            }

            static_cast<Response*>(ptr)->session =
                std::move(value);
        }

        return bytes;
    } catch (...) {
        return 0;
    }
}

// 解析 JSON-RPC 响应，并检查返回的 id。
//
// 阿里云 MCP 服务可能直接返回 JSON，
// 也可能在 SSE 的 data 行里返回 JSON。
json::object parseReply(
    const std::string& body,
    int id
) {
    auto match = [id](const std::string& text) -> json::object {
        boost::system::error_code ec;
        auto value = json::parse(text, ec);

        if (ec || !value.is_object()) {
            return {};
        }

        auto object = value.as_object();

        const auto* version = object.if_contains("jsonrpc");
        const auto* replyId = object.if_contains("id");

        if (
            !version ||
            !version->is_string() ||
            version->as_string() != "2.0" ||
            !replyId ||
            !replyId->is_int64() ||
            replyId->as_int64() != id
        ) {
            return {};
        }

        return object;
    };

    // 先尝试把整个响应当成 JSON。
    auto reply = match(body);

    if (!reply.empty()) {
        return reply;
    }

    // 如果不是普通 JSON，再查找 SSE 的 data 行。
    std::size_t start = 0;

    while (start < body.size()) {
        const auto end = body.find('\n', start);

        auto line = body.substr(
            start,
            end == std::string::npos
                ? end
                : end - start
        );

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.rfind("data:", 0) == 0) {
            reply = match(line.substr(5));

            if (!reply.empty()) {
                return reply;
            }
        }

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }

    throw std::runtime_error("invalid MCP response");
}

// 一个对象对应一次搜索使用的 MCP 会话。
class McpClient {
public:
    McpClient(
        std::string url,
        std::string key
    )
        : url_(std::move(url))
        , key_(std::move(key)) {
        if (
            url_.rfind("https://", 0) != 0 ||
            url_.find_first_of("\r\n") != std::string::npos ||
            key_.empty() ||
            key_.find_first_of("\r\n") != std::string::npos
        ) {
            throw std::invalid_argument(
                "invalid MCP configuration"
            );
        }
    }

    // 发送一个 JSON-RPC 消息。
    //
    // notification 为 true 时，该消息是通知，
    // 不要求返回 JSON-RPC 结果。
    json::object post(
        const json::object& request,
        int id,
        bool notification = false
    ) {
        using Curl =
            std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;

        Curl curl(curl_easy_init(), &curl_easy_cleanup);

        if (!curl) {
            throw std::runtime_error(
                "MCP curl initialization failed"
            );
        }

        // 自动管理 curl 请求头链表。
        struct Headers {
            curl_slist* list = nullptr;

            ~Headers() {
                curl_slist_free_all(list);
            }

            void add(const std::string& value) {
                auto* next = curl_slist_append(
                    list,
                    value.c_str()
                );

                if (!next) {
                    throw std::runtime_error(
                        "MCP header allocation failed"
                    );
                }

                list = next;
            }
        };

        Headers headers;

        headers.add("Content-Type: application/json");
        headers.add(
            "Accept: application/json, text/event-stream"
        );
        headers.add("Authorization: Bearer " + key_);

        if (!session_.empty()) {
            headers.add("Mcp-Session-Id: " + session_);
        }

        if (!version_.empty()) {
            headers.add("MCP-Protocol-Version: " + version_);
        }

        const std::string data = json::serialize(request);
        Response response;

        curl_easy_setopt(curl.get(), CURLOPT_URL, url_.c_str());

        curl_easy_setopt(
            curl.get(),
            CURLOPT_HTTPHEADER,
            headers.list
        );

        curl_easy_setopt(
            curl.get(),
            CURLOPT_POSTFIELDS,
            data.c_str()
        );

        curl_easy_setopt(
            curl.get(),
            CURLOPT_POSTFIELDSIZE,
            static_cast<long>(data.size())
        );

        curl_easy_setopt(
            curl.get(),
            CURLOPT_WRITEFUNCTION,
            writeBody
        );

        curl_easy_setopt(
            curl.get(),
            CURLOPT_WRITEDATA,
            &response
        );

        curl_easy_setopt(
            curl.get(),
            CURLOPT_HEADERFUNCTION,
            readHeader
        );

        curl_easy_setopt(
            curl.get(),
            CURLOPT_HEADERDATA,
            &response
        );

        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 8L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 25L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);

        // 不跟随重定向，避免将鉴权信息转发到其他地址。
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);

        // 与原项目保持一致：MCP 请求直连，不使用代理。
        curl_easy_setopt(curl.get(), CURLOPT_PROXY, "");

        if (curl_easy_perform(curl.get()) != CURLE_OK) {
            throw std::runtime_error("MCP request failed");
        }

        long status = 0;

        if (
            curl_easy_getinfo(
                curl.get(),
                CURLINFO_RESPONSE_CODE,
                &status
            ) != CURLE_OK ||
            status < 200 ||
            status >= 300
        ) {
            throw std::runtime_error("MCP HTTP error");
        }

        if (!response.session.empty()) {
            session_ = std::move(response.session);
        }

        if (notification) {
            return {};
        }

        auto reply = parseReply(response.body, id);
        const auto* result = reply.if_contains("result");

        if (
            reply.if_contains("error") ||
            !result ||
            !result->is_object()
        ) {
            throw std::runtime_error("MCP RPC error");
        }

        return result->as_object();
    }

    // 建立 MCP 会话并发送初始化完成通知。
    void initialize() {
        auto request = json::parse(
            R"({
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2025-03-26",
                    "capabilities": {},
                    "clientInfo": {
                        "name": "Ai_http_cpp",
                        "version": "1.0"
                    }
                }
            })"
        ).as_object();

        auto result = post(request, 1);

        const auto* version =
            result.if_contains("protocolVersion");

        if (!version || !version->is_string()) {
            throw std::runtime_error(
                "missing MCP protocol version"
            );
        }

        version_ = std::string(version->as_string());

        if (
            version_ != "2025-03-26" &&
            version_ != "2024-11-05"
        ) {
            throw std::runtime_error(
                "unsupported MCP version"
            );
        }

        auto notification = json::parse(
            R"({
                "jsonrpc": "2.0",
                "method": "notifications/initialized"
            })"
        ).as_object();

        post(notification, 0, true);
    }

private:
    std::string url_;
    std::string key_;

    // MCP 服务返回的会话 ID。
    std::string session_;

    // 初始化阶段协商得到的协议版本。
    std::string version_;
};

} // namespace

// 判断 MCP 配置是否存在。
bool mcpSearchConfigured() {
    const char* url = std::getenv("MCP_SERVER_URL");
    const char* key = std::getenv("DASHSCOPE_API_KEY");

    return url && *url && key && *key;
}

// 执行一次阿里云 MCP 搜索。
//
// 这里只允许调用固定的：
// WebSearch/bailian_web_search
std::string mcpWebSearch(
    const boost::json::object& args
) {
    const auto* queryValue = args.if_contains("query");

    if (
        args.size() != 1 ||
        !queryValue ||
        !queryValue->is_string()
    ) {
        throw std::invalid_argument(
            "web_search requires query"
        );
    }

    const auto& q = queryValue->as_string();
    const std::string query(q.data(), q.size());

    if (
        query.empty() ||
        query.size() > 1500 ||
        query.find('\0') != std::string::npos
    ) {
        throw std::invalid_argument(
            "invalid search query"
        );
    }

    if (!mcpSearchConfigured()) {
        throw std::runtime_error(
            "MCP search is not configured"
        );
    }

    McpClient client(
        std::getenv("MCP_SERVER_URL"),
        std::getenv("DASHSCOPE_API_KEY")
    );

    // 第一步：建立 MCP 会话。
    client.initialize();

    // 第二步：调用阿里云固定的搜索工具。
    auto request = json::parse(
        R"({
            "jsonrpc": "2.0",
            "id": 2,
            "method": "tools/call",
            "params": {
                "name": "call_tool",
                "arguments": {
                    "server_code": "WebSearch",
                    "tool_name": "bailian_web_search",
                    "params": {
                        "query": "",
                        "count": 3
                    }
                }
            }
        })"
    ).as_object();

    request.at("params")
        .as_object()
        .at("arguments")
        .as_object()
        .at("params")
        .as_object()["query"] = query;

    auto result = client.post(request, 2);

    const auto* isError = result.if_contains("isError");

    if (
        isError &&
        (!isError->is_bool() || isError->as_bool())
    ) {
        throw std::runtime_error("MCP tool error");
    }

    const auto* content = result.if_contains("content");

    if (
        !content ||
        !content->is_array() ||
        content->as_array().empty()
    ) {
        throw std::runtime_error(
            "MCP returned no content"
        );
    }

    // 把搜索结果作为字符串回传给模型。
    const auto output = json::serialize(result);

    if (output.size() > 32000) {
        throw std::runtime_error(
            "MCP result too large"
        );
    }

    return output;
}