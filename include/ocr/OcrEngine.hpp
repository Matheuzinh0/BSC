/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace ofx::ocr {

struct GlyphBox {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
};

struct TextLine {
    std::string text;
    int yTop{0};
    int yBottom{0};
    float confidence{1.0f};
};

class OcrEngine {
public:
    OcrEngine();
    ~OcrEngine() = default;

    static OcrEngine& instance();

    // Check if an external OCR engine (such as Tesseract) is installed in system PATH
    bool isSystemOcrAvailable() const;

    // Perform OCR directly on image file (PNG, JPG, BMP, PPM)
    std::string recognizeImageFile(const std::string& imagePath) const;

    // Perform OCR on raw compressed image memory (JPEG, PNG, BMP)
    std::string recognizeImageMemory(const uint8_t* data, size_t size) const;

    // Perform native C++ OCR on 8-bit grayscale pixel buffer
    std::string recognizeGrayscale(const uint8_t* grayPixels, int width, int height) const;

    // Extract raster pages from PDF (via embedded streams or pdftoppm) and execute OCR
    std::string extractAndRecognizePdf(const std::string& pdfPath, const std::vector<uint8_t>& pdfBytes) const;

    // Extract raw embedded image byte streams (JPEG / Flate) directly from PDF binary buffer
    std::vector<std::vector<uint8_t>> extractImagesFromPdf(const std::vector<uint8_t>& pdfBytes) const;

    // Contextual domain correction for Brazilian financial statements
    std::string postProcessFinancialText(const std::string& rawText) const;

private:
    bool m_hasTesseract{false};

    // Internal Native C++ OCR Pipeline stages
    std::vector<uint8_t> binarizeAdaptive(const uint8_t* gray, int w, int h) const;
    std::vector<GlyphBox> segmentLines(const uint8_t* binary, int w, int h) const;
    std::vector<GlyphBox> segmentGlyphsInLine(const uint8_t* binary, int w, int h, const GlyphBox& lineBox, std::vector<bool>& spaceBefore) const;
    char classifyGlyph(const uint8_t* binary, int imgW, const GlyphBox& box) const;
    int countClosedLoops(const uint8_t* matrix16x16) const;

    std::string runTesseract(const std::string& imagePath) const;
};

} // namespace ofx::ocr
