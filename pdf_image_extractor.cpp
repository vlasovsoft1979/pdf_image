#include "pdf_image_extractor.h"

#include <cstring>
#include <stdexcept>
#include <vector>
#include <map>
#include <pdfio.h>

#include "i_image_stream.h"

enum class StreamType
{
    Unknown = 0,
    FlateDecode = 1,
    DCTDecode = 2,
};

enum class ColorSpace
{
    Unknown = 0,
    DeviceGray = 1,
    DeviceRGB = 2,
};

class PdfImageZlibStream: public IImageStream
{
public:
    PdfImageZlibStream(pdfio_obj_t* obj, unsigned int width, unsigned int height, ColorSpace colorSpace)
        : m_obj(obj)
        , m_width(width)
        , m_height(height)
        , m_colorSpace(colorSpace)
    {}
    unsigned int GetWidth() const override
    {
        return m_width;
    }
    unsigned int GetHeight() const override
    {
        return m_height;
    }
    void Extract(void* buffer) override
    {
        std::unique_ptr<pdfio_stream_t, decltype(&pdfioStreamClose)> streamHolder{ pdfioObjOpenStream(m_obj, true), pdfioStreamClose };
        switch(m_colorSpace)
        {
        case ColorSpace::DeviceGray:
            ExtractGray(streamHolder.get(), buffer);
            break;
        case ColorSpace::DeviceRGB:
            ExtractRGB(streamHolder.get(), buffer);
            break;
        default:
            throw std::runtime_error("Unknown color space");
        }
    }
private:
    void ExtractGray(pdfio_stream_t* stream, void* buffer);
    void ExtractRGB(pdfio_stream_t* stream, void* buffer);
private:
    pdfio_obj_t* m_obj;
    unsigned int m_width;
    unsigned int m_height;
    ColorSpace m_colorSpace;

};

void PdfImageZlibStream::ExtractGray(pdfio_stream_t* stream, void* buffer)
{
    size_t expected_size = m_width * m_height;
    size_t scanline = m_width;
    uint8_t* buf = reinterpret_cast<uint8_t*>(buffer);
    uint8_t* ptr = buf;
    ssize_t cnt_read = 0;
    size_t total_read = 0;
    do
    {
        cnt_read = pdfioStreamRead(stream, ptr, scanline);
        if (cnt_read > 0)
        {
            if (cnt_read != scanline)
            {
                throw std::runtime_error("Error reading image stream (bad stream)");
            }
            ptr += cnt_read;
            total_read += cnt_read;
        }
    } while (cnt_read > 0 && expected_size > total_read);
    if (total_read != expected_size)
    {
        throw std::runtime_error("Error reading image stream (bad stream)");
    }
}

void PdfImageZlibStream::ExtractRGB(pdfio_stream_t* stream, void* buffer)
{
    size_t expected_size = m_width * m_height;
    size_t total_read = 0;
    size_t scanline = 3 * m_width;
    ssize_t cnt_read = 0;
    uint8_t* ptr = reinterpret_cast<uint8_t*>(buffer);
    std::unique_ptr<uint8_t[]> rgb(new uint8_t[scanline]);
    do
    {
        cnt_read = pdfioStreamRead(stream, rgb.get(), scanline);
        if (cnt_read > 0)
        {
            if (cnt_read != scanline)
            {
                throw std::runtime_error("Error reading image stream (bad stream)");
            }
            for (int count = 0; count < cnt_read/3; ++count)
            {
                *ptr = static_cast<uint8_t>(0.299 * rgb[3*count + 0] + 0.587 * rgb[3*count + 1] + 0.114 * rgb[3*count + 2]);
                ptr++;
            }
            total_read += cnt_read;
        }
    } while(cnt_read > 0 && 3*expected_size > total_read);
}

class PdfImageExtractor: public IImageExtractor
{
public:
    PdfImageExtractor(const char* file_name);
    ~PdfImageExtractor();
    virtual size_t GetImagesCount() const override
    {
        return images.size();
    }
    virtual IImageStream* GetImageStream(unsigned int index) const override
    {
        return images[index].get();
    }
protected:
    void Process();
    void AddStream(std::unique_ptr<IImageStream> stream);
protected:
    pdfio_file_t* pdf;
    std::vector<std::unique_ptr<IImageStream>> images;
};

PdfImageExtractor::PdfImageExtractor(const char* file_name)
    : pdf(pdfioFileOpen(file_name, nullptr, nullptr, nullptr, nullptr))
{
    if (!pdf)
    {
        throw std::runtime_error("Error opening PDF file");
    }
    Process();
}

PdfImageExtractor::~PdfImageExtractor()
{
    pdfioFileClose(pdf);
}

static StreamType GetStreamType(const char* str)
{
    if (str)
    {
        if (0 == strcmp(str, "FlateDecode"))
        {
            return StreamType::FlateDecode;
        }
        else if (0 == strcmp(str, "DCTDecode"))
        {
            return StreamType::DCTDecode;
        }
    }
    return StreamType::Unknown;
}

static StreamType GetStreamType(pdfio_dict_t* dict)
{
    auto type = pdfioDictGetType(dict, "Filter");
    if (type == PDFIO_VALTYPE_NAME)
    {
        return GetStreamType(pdfioDictGetName(dict, "Filter"));
    }
    else if (type == PDFIO_VALTYPE_ARRAY)
    {
        auto arr = pdfioDictGetArray(dict, "Filter");
        if (1 == pdfioArrayGetSize(arr))
        {
            return GetStreamType(pdfioArrayGetName(arr, 0));
        }
    }
    return StreamType::Unknown;
}

static ColorSpace GetColorSpace(const char* str)
{
    if (str)
    {
        if (0 == strcmp(str, "DeviceGray"))
        {
            return ColorSpace::DeviceGray;
        }
        else if (0 == strcmp(str, "DeviceRGB"))
        {
            return ColorSpace::DeviceRGB;
        }
    }
    return ColorSpace::Unknown;
}

static ColorSpace GetColorSpace(pdfio_dict_t* dict)
{
    auto type = pdfioDictGetType(dict, "ColorSpace");
    if (PDFIO_VALTYPE_NAME == type)
    {
        return GetColorSpace(pdfioDictGetName(dict, "ColorSpace"));
    }
    else if (PDFIO_VALTYPE_INDIRECT == type)
    {
        pdfio_obj_t* obj = pdfioDictGetObj(dict, "ColorSpace");
        if (obj)
        {
            return GetColorSpace(pdfioObjGetName(obj));
        }
    }
    return ColorSpace::Unknown;
}

struct ImageParams
{
    int width;
    int height;
    int bpp;
    StreamType streamType;
    ColorSpace colorSpace;
    ImageParams()
        : width(0)
        , height(0)
        , bpp(0)
        , streamType(StreamType::Unknown)
        , colorSpace(ColorSpace::Unknown)
    {}
};

static bool GetImageParams(pdfio_obj_t* obj, ImageParams& params)
{
    const char* type = pdfioObjGetType(obj);
    if (0 != strcmp(type, "XObject"))
        return false;

    const char* stype = pdfioObjGetSubtype(obj);
    if (0 != strcmp(stype, "Image"))
        return false;

    pdfio_dict_t* dict = pdfioObjGetDict(obj);
    if (!dict)
        return false;

    params.streamType = GetStreamType(dict);
    params.colorSpace = GetColorSpace(dict);

    params.width = static_cast<int>(pdfioDictGetNumber(dict, "Width"));
    params.height = static_cast<int>(pdfioDictGetNumber(dict, "Height"));
    params.bpp = static_cast<int>(pdfioDictGetNumber(dict, "BitsPerComponent"));
 
    return true;
}

static pdfio_dict_t* GetResources(pdfio_obj_t* obj)
{
    pdfio_dict_t* dict = pdfioObjGetDict(obj);
    if (!dict)
        return nullptr;

    auto type = pdfioDictGetType(dict, "Resources");
    if (PDFIO_VALTYPE_DICT == type)
    {
        return pdfioDictGetDict(dict, "Resources");
    }
    else if (PDFIO_VALTYPE_INDIRECT == type)
    {
        pdfio_obj_t* obj = pdfioDictGetObj(dict, "Resources");
        if (obj)
            return pdfioObjGetDict(obj);
    }
    return nullptr;
}

static bool PdfCallback(pdfio_dict_t* dict, const char* key, void* cb_data)
{
    pdfio_obj_t* obj = pdfioDictGetObj(dict, key);
    ImageParams imageParams;
    if (!GetImageParams(obj, imageParams))
        return true;

    if (imageParams.width > 0 && imageParams.height > 0 && imageParams.bpp == 8 && StreamType::FlateDecode == imageParams.streamType)
    {
        auto* images = reinterpret_cast<std::map<pdfio_obj_t*, ImageParams>*>(cb_data);
        images->insert(std::make_pair(obj, imageParams));

        PdfImageExtractor* extractor = reinterpret_cast<PdfImageExtractor*>(cb_data);
    }
    return true;
}

static void CollectImages(pdfio_obj_t* obj, std::map<pdfio_obj_t*, ImageParams>& images)
{
    pdfio_dict_t* resources = GetResources(obj);
    if (!resources)
        return;
    pdfio_dict_t* xobject = pdfioDictGetDict(resources, "XObject");
    if (!xobject)
        return;
    pdfioDictIterateKeys(xobject, PdfCallback, &images);
}

void PdfImageExtractor::Process()
{
    std::map<pdfio_obj_t*, ImageParams> images;

    pdfio_dict_t* catalog = pdfioFileGetCatalog(pdf);
    if (!catalog)
        return;

    pdfio_obj_t* pages = pdfioDictGetObj(catalog, "Pages");
    if (!pages)
        return;

    CollectImages(pages, images);

    auto pagesCount = pdfioFileGetNumPages(pdf);
    for (auto pageNo = 0u; pageNo < pagesCount; ++pageNo)
    {
        pdfio_obj_t* page = pdfioFileGetPage(pdf, pageNo);
        if (!page)
            continue;

        CollectImages(page, images);
    }

    for (auto image: images)
    {
        const ImageParams& imageParams = image.second;
        switch(imageParams.streamType)
        {
        case StreamType::FlateDecode:
            AddStream(std::unique_ptr<IImageStream>(
                new PdfImageZlibStream(image.first,
                    imageParams.width,
                    imageParams.height,
                    imageParams.colorSpace)));
            break;
        default:
            break;
        }
    }
}

void PdfImageExtractor::AddStream(std::unique_ptr<IImageStream> stream)
{
    images.push_back(std::move(stream));
}

IImageExtractorPtr CreatePfdImageExtractor(const char* file_name)
{
    return std::unique_ptr<IImageExtractor>(new PdfImageExtractor(file_name));
}