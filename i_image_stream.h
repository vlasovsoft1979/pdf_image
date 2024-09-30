#pragma once

class IImageStream
{
public:
    virtual ~IImageStream() {}
    virtual unsigned int GetWidth() const = 0;
    virtual unsigned int GetHeight() const = 0;
    virtual void Extract(void* buffer) = 0;
};
