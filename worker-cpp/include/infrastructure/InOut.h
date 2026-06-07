#ifndef HARM_INOUT_H
#define HARM_INOUT_H

#include <string>

class IJobInput {
public:
    virtual ~IJobInput() = default;
    virtual std::string readNextJob() = 0;
};

class IResultOutput {
public:
    virtual ~IResultOutput() = default;
    virtual void writeResult(const std::string& resultJson) = 0;
};

class InOut : public IJobInput, public IResultOutput {
public:
    std::string readNextJob() override;
    void writeResult(const std::string& resultJson) override;
    bool isShutdownCommand(const std::string& input) const;
};

#endif
