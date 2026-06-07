#include "Results.h"
#include <sstream>

static std::string escapeJsonString(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

JobResult JobResult::success(const std::string& jobId) {
    return success(jobId, {});
}

JobResult JobResult::success(const std::string& jobId, const std::vector<StoredResultFile>& files) {
    JobResult r;
    r.jobId  = jobId;
    r.status = "success";
    r.files  = files;
    return r;
}

JobResult JobResult::error(const std::string& jobId, const std::string& code, const std::string& message) {
    JobResult r;
    r.jobId  = jobId;
    r.status = "error";
    r.errors.push_back({code, message});
    return r;
}

JobResult JobResult::error(const std::string& code, const std::string& message) {
    return error("", code, message);
}

std::string JobResult::toJson() const {
    std::ostringstream out;
    out << "{";
    out << "\"jobId\":\"" << escapeJsonString(jobId) << "\",";
    out << "\"status\":\"" << escapeJsonString(status) << "\",";

    out << "\"results\":[";
    for (size_t i = 0; i < files.size(); ++i) {
        if (i > 0) out << ",";
        out << "{"
            << "\"variantId\":\"" << escapeJsonString(files[i].variantId) << "\","
            << "\"filePath\":\""  << escapeJsonString(files[i].path)      << "\","
            << "\"score\":"       << files[i].score
            << "}";
    }
    out << "],";

    out << "\"errors\":[";
    for (size_t i = 0; i < errors.size(); ++i) {
        if (i > 0) out << ",";
        out << "{"
            << "\"code\":\""    << escapeJsonString(errors[i].code)    << "\","
            << "\"message\":\"" << escapeJsonString(errors[i].message) << "\""
            << "}";
    }
    out << "]";

    out << "}";
    return out.str();
}
