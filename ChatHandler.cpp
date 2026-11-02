#include "ChatHandler.h"

#include <boost/json/src.hpp>

HttpResponse ChatHandler::handle(
    const HttpRequest& request
){
     boost::system::error_code ec;

    const auto data =
        boost::json::parse(request.body, ec);

    if (ec || !data.is_object()) {
        return HttpResponse(
            400,
            "Bad Request",
            R"({"error":"body must be a JSON object"})"
        );
    }

    const auto* question =
        data.as_object().if_contains("question");

    if (!question ||
        !question->is_string() ||
        question->as_string().empty()) {
        return HttpResponse(
            400,
            "Bad Request",
            R"({"error":"question must be a non-empty string"})"
        );
    }

    const std::string text(
        question->as_string().c_str(),
        question->as_string().size()
    );

    boost::json::object result;

    result["success"] = true;
    result["answer"] =
        "[模拟AI]收到你的信息：" + text;

    return HttpResponse(
        200,
        "OK",
        boost::json::serialize(result)
    );
}