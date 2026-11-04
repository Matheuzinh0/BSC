/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "ocr/OcrEngine.hpp"
#include "core/Utils.hpp"
#include "miniz/miniz.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <regex>
#include <array>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_PNM
#include "stb/stb_image.h"

namespace ofx::ocr {

struct TemplateEntry {
    char c;
    uint16_t rows[16];
    float ar;
};

static const TemplateEntry kTemplates[] = {
    { '0', {0x01c0,0x01c0,0x0ff8,0x3838,0x380e,0x380e,0x300e,0x3006,0x3006,0x3006,0x380e,0x380e,0x380e,0x0e38,0x07f0,0x0000}, 0.71f },
    { '1', {0x0000,0x0000,0x0f80,0x3f80,0x3380,0x0380,0x0380,0x0380,0x0380,0x0380,0x0380,0x0380,0x0380,0x0380,0x0380,0xfffe}, 0.69f },
    { '2', {0x0000,0x0000,0x0ff8,0x0838,0x380e,0x000e,0x000e,0x0008,0x0038,0x00f0,0x07c0,0x07c0,0x0f00,0x0e00,0x3800,0x3ffe}, 0.77f },
    { '3', {0x0000,0x0000,0x0ff8,0x3838,0x380e,0x000e,0x0038,0x01f0,0x01f8,0x01f8,0x000e,0x000e,0x380e,0x3e38,0x0ff8,0x0000}, 0.71f },
    { '4', {0x0000,0x0000,0x00f8,0x00f8,0x01f8,0x0738,0x0738,0x0638,0x0e38,0x3838,0x3038,0x3038,0xffff,0x0038,0x0038,0x0038}, 0.77f },
    { '5', {0x0000,0x0000,0x3ffe,0x3800,0x3800,0x3800,0x39c0,0x3ff8,0x000e,0x000e,0x000e,0x000e,0x380e,0x3e3e,0x0ff0,0x0000}, 0.71f },
    { '6', {0x00c0,0x00c0,0x07f8,0x0e0e,0x3800,0x3800,0x39f0,0x3ff8,0x380e,0x380e,0x380e,0x380e,0x380e,0x0f38,0x07f0,0x0000}, 0.71f },
    { '7', {0x0000,0x0000,0x3ffe,0x000e,0x0008,0x0038,0x0038,0x00f0,0x00f0,0x01c0,0x01c0,0x01c0,0x0100,0x0100,0x0700,0x0700}, 0.77f },
    { '8', {0x0000,0x0000,0x0ff8,0x3808,0x380e,0x380e,0x0e38,0x07f0,0x0e38,0x0e38,0x380e,0x380e,0x380e,0x3e38,0x0ff8,0x0000}, 0.71f },
    { '9', {0x0000,0x0000,0x0ff8,0x3838,0x380e,0x380e,0x380e,0x380e,0x0ffe,0x0ffe,0x01ce,0x000e,0x0808,0x0ef8,0x0ff0,0x0000}, 0.71f },
    { 'A', {0x0000,0x0000,0x01c0,0x03e0,0x03e0,0x0e30,0x0e30,0x0e30,0x0c10,0x1c1c,0x1ffc,0x1ffc,0x3ffe,0x300e,0x2002,0xe003}, 0.92f },
    { 'B', {0x0000,0x0000,0xfff8,0xf00e,0xf00e,0xf00e,0xf00e,0xf03e,0xfff8,0xf03e,0xf007,0xf007,0xf007,0xf007,0xf83e,0xfff8}, 0.77f },
    { 'C', {0x0080,0x0080,0x0ff8,0x1e0c,0x1806,0x3000,0x3000,0x3000,0x3000,0x3000,0x3000,0x3002,0x1806,0x0f3c,0x07f8,0x0000}, 0.93f },
    { 'D', {0x0000,0x0000,0xffe0,0xe03c,0xe01c,0xe00e,0xe00e,0xe00e,0xe00e,0xe00e,0xe00e,0xe00e,0xe00e,0xe01c,0xf1f0,0xffe0}, 0.92f },
    { 'E', {0x0000,0x0000,0xfffe,0xe000,0xe000,0xe000,0xe000,0xe000,0xfffc,0xf800,0xe000,0xe000,0xe000,0xe000,0xf800,0xfffe}, 0.85f },
    { 'F', {0x0000,0x0000,0xfffe,0xf000,0xf000,0xf000,0xf000,0xf000,0xf800,0xfffe,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000}, 0.77f },
    { 'G', {0x00c0,0x00c0,0x0ffc,0x1e0e,0x1806,0x3000,0x3000,0x3000,0x307f,0x307f,0x3003,0x3003,0x1803,0x0f3e,0x07fc,0x0000}, 0.93f },
    { 'H', {0x0000,0x0000,0xe003,0xe003,0xe003,0xe003,0xe003,0xe003,0xffff,0xe00f,0xe003,0xe003,0xe003,0xe003,0xe003,0xe003}, 0.85f },
    { 'I', {0x0000,0x0000,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0}, 0.23f },
    { 'J', {0x0000,0x0000,0x03ff,0x000f,0x000f,0x000f,0x000f,0x000f,0x000f,0x000f,0x000f,0x000f,0xf00c,0x3cfc,0x0ff0,0x0000}, 0.57f },
    { 'K', {0x0000,0x0000,0xe01c,0xe070,0xe0e0,0xe380,0xe380,0xe700,0xff00,0xfb80,0xe0e0,0xe0e0,0xe070,0xe07c,0xe01e,0xe00e}, 0.85f },
    { 'L', {0x0000,0x0000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xffff}, 0.70f },
    { 'M', {0x0000,0x0000,0xe007,0xf00f,0xf81f,0xdc3b,0xce73,0xc663,0xc243,0xc003,0xc003,0xc003,0xc003,0xc003,0xc003,0xc003}, 1.15f },
    { 'N', {0x0000,0x0000,0xe007,0xf007,0xf807,0xdc07,0xce07,0xc707,0xc387,0xc1c7,0xc0e7,0xc077,0xc03f,0xc01f,0xc00f,0xc007}, 0.85f },
    { 'O', {0x0000,0x0000,0x07f0,0x1e3c,0x380e,0x7007,0x7007,0x7007,0x7007,0x7007,0x7007,0x7007,0x380e,0x1e3c,0x07f0,0x0000}, 0.95f },
    { 'P', {0x0000,0x0000,0xffe0,0xf038,0xf01c,0xf00e,0xf00e,0xf01c,0xf038,0xffe0,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000}, 0.77f },
    { 'Q', {0x0000,0x0000,0x07f0,0x1e3c,0x380e,0x7007,0x7007,0x7007,0x7007,0x7007,0x7067,0x70ef,0x39ef,0x1e7c,0x07f3,0x0007}, 0.95f },
    { 'R', {0x0000,0x0000,0xffe0,0xf038,0xf01c,0xf00e,0xf00e,0xf01c,0xf038,0xffe0,0xf380,0xf1c0,0xf0e0,0xf070,0xf038,0xf01c}, 0.85f },
    { 'S', {0x0000,0x0000,0x0fe0,0x1818,0x300c,0x3000,0x1800,0x0fc0,0x01f0,0x0038,0x001c,0x300c,0x300c,0x1818,0x0fe0,0x0000}, 0.77f },
    { 'T', {0x0000,0x0000,0xffff,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0}, 0.77f },
    { 'U', {0x0000,0x0000,0xe007,0xe007,0xe007,0xe007,0xe007,0xe007,0xe007,0xe007,0xe007,0xe007,0x700e,0x381c,0x0ff0,0x0000}, 0.85f },
    { 'V', {0x0000,0x0000,0xe007,0xe007,0xe007,0x700e,0x700e,0x381c,0x381c,0x381c,0x1c38,0x1c38,0x0e70,0x0e70,0x07e0,0x03c0}, 0.85f },
    { 'W', {0x0000,0x0000,0xc003,0xc003,0xc003,0x6186,0x6186,0x63c6,0x324c,0x324c,0x366c,0x1a58,0x1c38,0x1c38,0x0c30,0x0810}, 1.25f },
    { 'X', {0x0000,0x0000,0xe007,0x700e,0x381c,0x1c38,0x0e70,0x07e0,0x03c0,0x07e0,0x0e70,0x1c38,0x381c,0x700e,0xe007,0x0000}, 0.85f },
    { 'Y', {0x0000,0x0000,0xe007,0x700e,0x381c,0x1c38,0x0e70,0x07e0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0,0x03c0}, 0.85f },
    { 'Z', {0x0000,0x0000,0xffff,0x000e,0x001c,0x0038,0x0070,0x00e0,0x01c0,0x0380,0x0700,0x0e00,0x1c00,0x3800,0x7000,0xffff}, 0.77f },
    { 'a', {0x0000,0x0000,0x0000,0x0000,0x0fe0,0x0038,0x001c,0x0ffc,0x381c,0x701c,0x701c,0x703c,0x3c7e,0x0fe7,0x0000,0x0000}, 0.65f },
    { 'b', {0xf000,0xf000,0xf000,0xf000,0xff80,0xf1f0,0xf038,0xf01c,0xf01c,0xf01c,0xf01c,0xf038,0xf1f0,0xfe60,0x0000,0x0000}, 0.65f },
    { 'c', {0x0000,0x0000,0x0000,0x0000,0x07f0,0x1c1c,0x380e,0x7000,0x7000,0x7000,0x7000,0x3804,0x1c1e,0x07f0,0x0000,0x0000}, 0.60f },
    { 'd', {0x0007,0x0007,0x0007,0x0007,0x0387,0x0fff,0x3c1f,0x3c07,0x3c07,0x3c07,0x3c07,0x3c07,0x3c07,0x3f7f,0x0fe7,0x0000}, 0.64f },
    { 'e', {0x01c0,0x01c0,0x0ff8,0x380e,0x380e,0x380e,0x3ffe,0x3ffe,0x3ffe,0x3800,0x3800,0x3800,0x0e3e,0x0e3e,0x07f8,0x0000}, 0.91f },
    { 'f', {0x01ff,0x01ff,0x01f8,0x01c0,0x0fff,0xffff,0xffff,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0}, 0.38f },
    { 'g', {0x0380,0x0380,0x0fff,0x3c1f,0x3c07,0x3c07,0x3c07,0x3c07,0x3c07,0x3c07,0x3f1f,0x0fe7,0x0007,0x3c07,0x0ffe,0x03f8}, 0.64f },
    { 'h', {0xf000,0xf000,0xf000,0xf000,0xf0c0,0xfffc,0xfffc,0xfc0f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f}, 0.62f },
    { 'i', {0xffff,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff}, 0.15f },
    { 'j', {0x00ff,0x00ff,0x0000,0x0000,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x0fff}, 0.24f },
    { 'k', {0xf000,0xf000,0xf000,0xf000,0xf00c,0xf03c,0xf03c,0xf0f0,0xf3c0,0xff00,0xffc0,0xffc0,0xf3c0,0xf0f0,0xf03c,0xf00f}, 0.62f },
    { 'l', {0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff}, 0.15f },
    { 'm', {0x060c,0x060c,0xffbe,0xffbe,0xf1c3,0xe0c3,0xe0c3,0xe083,0xe083,0xe083,0xe083,0xe083,0xe083,0xe083,0xe083,0xe083}, 1.30f },
    { 'n', {0x00c0,0x00c0,0xfffc,0xfffc,0xfc0f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f}, 0.80f },
    { 'o', {0x01c0,0x01c0,0x0ff8,0x380e,0x380e,0x380e,0x380e,0x380e,0x380e,0x380e,0x380e,0x380e,0x3e38,0x3e38,0x0ff0,0x0000}, 0.91f },
    { 'p', {0x00e0,0x00e0,0xfff8,0xfc1e,0xf01e,0xf01e,0xf01e,0xf01e,0xf01e,0xf01e,0xfc7e,0xf3f8,0xf000,0xf000,0xf000,0xf000}, 0.64f },
    { 'q', {0x0387,0x0387,0x0fff,0x3c1f,0x3c07,0x3c07,0x3c07,0x3c07,0x3c07,0x3c07,0x3f7f,0x0fe7,0x0007,0x0007,0x0007,0x0007}, 0.64f },
    { 'r', {0x0038,0x0038,0xffff,0xffff,0xffc0,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00}, 0.50f },
    { 's', {0x0080,0x0080,0x3ff8,0x3c1e,0x3c1e,0x3c00,0x3f80,0x3f80,0x03f8,0x001e,0x001e,0x3006,0x3c1e,0x3c1e,0x0ff8,0x0000}, 0.82f },
    { 't', {0x01c0,0x01c0,0x01c0,0x0ff8,0xffff,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x01ff,0x0000}, 0.38f },
    { 'u', {0x0000,0x0000,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0xf00f,0x3c3f,0x3c3f,0x3fcf,0x0000}, 0.73f },
    { 'v', {0x0000,0x0000,0xf007,0xf007,0x3c1e,0x3c1e,0x3c1e,0x3c1e,0x0f78,0x0f78,0x0f78,0x0f78,0x0f60,0x03e0,0x03e0,0x03e0}, 0.90f },
    { 'w', {0x20c1,0x20c1,0x31e3,0x31e3,0x31e3,0x1122,0x1122,0x1b26,0x1b36,0x1b36,0x0a34,0x0a34,0x0e1c,0x0e1c,0x0e1c,0x0e1c}, 1.40f },
    { 'x', {0x3006,0x3006,0x3c1e,0x3c1e,0x0c78,0x0f78,0x0f78,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x0f78,0x3c1e,0x3c1e,0xf007}, 0.90f },
    { 'y', {0xc000,0xc000,0xf007,0x3c1e,0x3c1e,0x0c1e,0x0f78,0x0f78,0x03e0,0x03e0,0x03e0,0x00e0,0x0380,0x0380,0x3f00,0x3c00}, 0.64f },
    { 'z', {0x3ffe,0x3ffe,0x3ffe,0x3ffe,0x0078,0x0078,0x0078,0x00e0,0x0380,0x0380,0x0f00,0x0f00,0x0f00,0x3c00,0x3c00,0x3ffe}, 0.90f },
    { '/', {0x003f,0x003f,0x003f,0x0038,0x0038,0x01f8,0x01f8,0x01c0,0x0fc0,0x0fc0,0x0fc0,0x0fc0,0x0e00,0xfe00,0xfe00,0x0000}, 0.36f },
    { '.', {0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0}, 1.50f },
    { '-', {0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc,0x1ffc}, 3.00f },
    { '$', {0x0000,0x01c0,0x0ff8,0x3ffe,0x39c6,0x39c0,0x3fc0,0x0ff0,0x01fe,0x01c7,0x31c7,0x31c6,0x3ffe,0x07f0,0x01c0,0x0000}, 0.62f },
    { ',', {0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x03e0,0x0000,0x0000,0x0000}, 0.60f },
    { ':', {0x03e0,0x03e0,0x03e0,0x03e0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x03e0,0x03e0,0x03e0}, 0.30f },
    { '(', {0x003f,0x01f8,0x01c0,0x0fc0,0x0e00,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00,0xfe00,0x0e00,0x0fc0,0x0fc0,0x01f8,0x01f8}, 0.29f },
    { ')', {0xfe00,0x0fc0,0x01c0,0x01f8,0x01f8,0x0038,0x003f,0x003f,0x003f,0x003f,0x003f,0x0038,0x01f8,0x01f8,0x0fc0,0x0fc0}, 0.29f },
    { '#', {0x0000,0x0000,0x0706,0x060e,0x0608,0x3fff,0x3fff,0x0e38,0x0e38,0x0830,0xfffe,0xfffe,0xfffe,0x0830,0x30f0,0x30c0}, 0.77f },
    { '+', {0x0000,0x0000,0x01c0,0x01c0,0x01c0,0x01c0,0x01c0,0x3ffe,0x3ffe,0x3ffe,0x01c0,0x01c0,0x01c0,0x01c0,0x01c0,0x0000}, 1.00f },
    { '%', {0x0000,0x0000,0x7810,0x4c30,0x4460,0x4440,0x44c8,0x4db6,0x7922,0x7922,0x0322,0x0622,0x0422,0x0c36,0x181c,0x0000}, 1.14f }
};

OcrEngine::OcrEngine() {
    int ret = std::system("which tesseract > /dev/null 2>&1");
    m_hasTesseract = (ret == 0);
}

OcrEngine& OcrEngine::instance() {
    static OcrEngine s_instance;
    return s_instance;
}

bool OcrEngine::isSystemOcrAvailable() const {
    return m_hasTesseract;
}

std::string OcrEngine::runTesseract(const std::string& imagePath) const {
    std::string outPrefix = "/tmp/bsc_tess_" + std::to_string(std::rand());
    std::string cmd = "tesseract \"" + imagePath + "\" \"" + outPrefix + "\" --oem 1 -l por+eng --psm 6 > /dev/null 2>&1";
    int ret = std::system(cmd.c_str());
    if (ret == 0) {
        std::string txtFile = outPrefix + ".txt";
        std::string result = core::Utils::readFileString(txtFile);
        std::remove(txtFile.c_str());
        if (!result.empty()) {
            return postProcessFinancialText(result);
        }
    }
    return "";
}

std::string OcrEngine::recognizeImageFile(const std::string& imagePath) const {
    if (m_hasTesseract) {
        std::string res = runTesseract(imagePath);
        if (!res.empty()) return res;
    }

    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(imagePath.c_str(), &w, &h, &channels, 1);
    if (!data) return "";

    std::string text = recognizeGrayscale(data, w, h);
    stbi_image_free(data);
    return postProcessFinancialText(text);
}

std::string OcrEngine::recognizeImageMemory(const uint8_t* data, size_t size) const {
    if (!data || size == 0) return "";

    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &channels, 1);
    if (!pixels) return "";

    std::string text = recognizeGrayscale(pixels, w, h);
    stbi_image_free(pixels);
    return postProcessFinancialText(text);
}

std::vector<uint8_t> OcrEngine::binarizeAdaptive(const uint8_t* gray, int w, int h) const {
    std::vector<uint8_t> binary(w * h, 0);
    if (w <= 0 || h <= 0) return binary;

    // Build Integral Image for Bradley-Roth adaptive thresholding
    std::vector<uint64_t> integral((w + 1) * (h + 1), 0);
    for (int y = 0; y < h; ++y) {
        uint64_t rowSum = 0;
        for (int x = 0; x < w; ++x) {
            rowSum += gray[y * w + x];
            integral[(y + 1) * (w + 1) + (x + 1)] = integral[y * (w + 1) + (x + 1)] + rowSum;
        }
    }

    int radius = std::max(8, w / 32);
    int tPercent = 15; // 15% darker than local neighborhood implies ink

    for (int y = 0; y < h; ++y) {
        int y1 = std::max(0, y - radius);
        int y2 = std::min(h - 1, y + radius);
        for (int x = 0; x < w; ++x) {
            int x1 = std::max(0, x - radius);
            int x2 = std::min(w - 1, x + radius);

            uint64_t count = (x2 - x1 + 1) * (y2 - y1 + 1);
            uint64_t sum = integral[(y2 + 1) * (w + 1) + (x2 + 1)]
                         - integral[y1 * (w + 1) + (x2 + 1)]
                         - integral[(y2 + 1) * (w + 1) + x1]
                         + integral[y1 * (w + 1) + x1];

            uint32_t pixel = gray[y * w + x];
            // If pixel is darker than local threshold => Ink (1)
            if (static_cast<uint64_t>(pixel) * count * 100 < sum * (100 - tPercent)) {
                binary[y * w + x] = 1;
            } else {
                binary[y * w + x] = 0;
            }
        }
    }

    return binary;
}

std::vector<GlyphBox> OcrEngine::segmentLines(const uint8_t* binary, int w, int h) const {
    std::vector<GlyphBox> lines;
    if (w <= 0 || h <= 0) return lines;

    std::vector<int> rowInk(h, 0);
    for (int y = 0; y < h; ++y) {
        int count = 0;
        for (int x = 0; x < w; ++x) {
            if (binary[y * w + x]) count++;
        }
        rowInk[y] = count;
    }

    int inkThreshold = std::max(3, w / 150);
    bool inLine = false;
    int lineStart = 0;

    for (int y = 0; y < h; ++y) {
        if (rowInk[y] >= inkThreshold) {
            if (!inLine) {
                inLine = true;
                lineStart = y;
            }
        } else {
            if (inLine) {
                int lineEnd = y - 1;
                int lh = lineEnd - lineStart + 1;
                if (lh >= 6) { // Filter out single noise lines
                    GlyphBox box;
                    box.x = 0;
                    box.y = lineStart;
                    box.w = w;
                    box.h = lh;
                    lines.push_back(box);
                }
                inLine = false;
            }
        }
    }

    if (inLine) {
        int lineEnd = h - 1;
        int lh = lineEnd - lineStart + 1;
        if (lh >= 6) {
            GlyphBox box;
            box.x = 0;
            box.y = lineStart;
            box.w = w;
            box.h = lh;
            lines.push_back(box);
        }
    }

    return lines;
}

std::vector<GlyphBox> OcrEngine::segmentGlyphsInLine(const uint8_t* binary, int w, int /*h*/,
                                                     const GlyphBox& lineBox,
                                                     std::vector<bool>& spaceBefore) const {
    std::vector<GlyphBox> glyphs;
    spaceBefore.clear();

    std::vector<int> colInk(w, 0);
    for (int x = 0; x < w; ++x) {
        int cnt = 0;
        for (int y = lineBox.y; y < lineBox.y + lineBox.h; ++y) {
            if (binary[y * w + x]) cnt++;
        }
        colInk[x] = cnt;
    }

    bool inGlyph = false;
    int gStart = 0;
    int prevEnd = -1;
    std::vector<int> glyphWidths;

    std::vector<std::pair<int, int>> spans;
    for (int x = 0; x < w; ++x) {
        if (colInk[x] > 0) {
            if (!inGlyph) {
                inGlyph = true;
                gStart = x;
            }
        } else {
            if (inGlyph) {
                int gEnd = x - 1;
                spans.push_back({gStart, gEnd});
                glyphWidths.push_back(gEnd - gStart + 1);
                inGlyph = false;
            }
        }
    }
    if (inGlyph) {
        spans.push_back({gStart, w - 1});
        glyphWidths.push_back(w - gStart);
    }

    if (spans.empty()) return glyphs;

    float avgW = 10.0f;
    if (!glyphWidths.empty()) {
        float sum = 0;
        for (int gw : glyphWidths) sum += gw;
        avgW = sum / glyphWidths.size();
    }
    float spaceThreshold = std::max(4.0f, avgW * 0.55f);

    for (size_t i = 0; i < spans.size(); ++i) {
        int x1 = spans[i].first;
        int x2 = spans[i].second;

        // Find precise vertical bounds of the glyph
        int yMin = lineBox.y + lineBox.h;
        int yMax = lineBox.y;
        for (int y = lineBox.y; y < lineBox.y + lineBox.h; ++y) {
            for (int x = x1; x <= x2; ++x) {
                if (binary[y * w + x]) {
                    if (y < yMin) yMin = y;
                    if (y > yMax) yMax = y;
                }
            }
        }

        if (yMax >= yMin) {
            GlyphBox gb;
            gb.x = x1;
            gb.y = yMin;
            gb.w = x2 - x1 + 1;
            gb.h = yMax - yMin + 1;

            bool sp = false;
            if (prevEnd >= 0 && (x1 - prevEnd) > spaceThreshold) {
                sp = true;
            }

            glyphs.push_back(gb);
            spaceBefore.push_back(sp);
            prevEnd = x2;
        }
    }

    return glyphs;
}

int OcrEngine::countClosedLoops(const uint8_t* matrix16x16) const {
    // 18x18 grid padded with zeros
    uint8_t grid[18][18] = {0};
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            grid[y + 1][x + 1] = matrix16x16[y * 16 + x] ? 1 : 0;
        }
    }

    // Flood fill background 0s from (0,0)
    std::vector<std::pair<int, int>> queue;
    queue.push_back({0, 0});
    grid[0][0] = 2;

    while (!queue.empty()) {
        auto [cy, cx] = queue.back();
        queue.pop_back();

        const int dx[] = {0, 0, 1, -1};
        const int dy[] = {1, -1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            int ny = cy + dy[d];
            int nx = cx + dx[d];
            if (ny >= 0 && ny < 18 && nx >= 0 && nx < 18 && grid[ny][nx] == 0) {
                grid[ny][nx] = 2;
                queue.push_back({ny, nx});
            }
        }
    }

    // Count remaining 0 components (inner loops / holes)
    int holes = 0;
    for (int y = 0; y < 18; ++y) {
        for (int x = 0; x < 18; ++x) {
            if (grid[y][x] == 0) {
                holes++;
                // Fill this hole
                queue.push_back({y, x});
                grid[y][x] = 3;
                while (!queue.empty()) {
                    auto [cy, cx] = queue.back();
                    queue.pop_back();
                    const int dx[] = {0, 0, 1, -1};
                    const int dy[] = {1, -1, 0, 0};
                    for (int d = 0; d < 4; ++d) {
                        int ny = cy + dy[d];
                        int nx = cx + dx[d];
                        if (ny >= 0 && ny < 18 && nx >= 0 && nx < 18 && grid[ny][nx] == 0) {
                            grid[ny][nx] = 3;
                            queue.push_back({ny, nx});
                        }
                    }
                }
            }
        }
    }

    return holes;
}

char OcrEngine::classifyGlyph(const uint8_t* binary, int imgW, const GlyphBox& box) const {
    if (box.w <= 0 || box.h <= 0) return ' ';

    float ar = static_cast<float>(box.w) / static_cast<float>(box.h);

    // Resample glyph to 16x16 normalized matrix
    uint8_t m16[256] = {0};
    uint16_t rows16[16] = {0};

    for (int r = 0; r < 16; ++r) {
        int srcY = box.y + (r * box.h) / 16;
        if (srcY >= box.y + box.h) srcY = box.y + box.h - 1;
        uint16_t rowVal = 0;
        for (int c = 0; c < 16; ++c) {
            int srcX = box.x + (c * box.w) / 16;
            if (srcX >= box.x + box.w) srcX = box.x + box.w - 1;
            uint8_t val = binary[srcY * imgW + srcX];
            m16[r * 16 + c] = val;
            if (val) {
                rowVal |= (1 << (15 - c));
            }
        }
        rows16[r] = rowVal;
    }

    // Heuristics for punctuation based on bounding geometry
    if (box.h <= 5 && box.w <= 6) {
        return '.';
    }
    if (ar > 2.2f && box.h <= 6) {
        return '-';
    }
    if (ar < 0.35f && box.h >= 10) {
        // Vertical line: 1, l, I, |
        return '1';
    }

    int loops = countClosedLoops(m16);

    char bestChar = '?';
    int bestDist = 999999;

    for (const auto& tmpl : kTemplates) {
        // Topological penalty for loop mismatch
        int loopPenalty = 0;
        if (tmpl.c == '8' || tmpl.c == 'B') {
            if (loops != 2) loopPenalty = 35;
        } else if (tmpl.c == '0' || tmpl.c == '6' || tmpl.c == '9' || tmpl.c == 'O' ||
                   tmpl.c == 'D' || tmpl.c == 'P' || tmpl.c == 'Q' || tmpl.c == 'R' ||
                   tmpl.c == 'A' || tmpl.c == 'a' || tmpl.c == 'b' || tmpl.c == 'd' ||
                   tmpl.c == 'e' || tmpl.c == 'g' || tmpl.c == 'o' || tmpl.c == 'p' || tmpl.c == 'q') {
            if (loops != 1) loopPenalty = 25;
        } else {
            if (loops > 0) loopPenalty = 30;
        }

        // Hamming distance between 16x16 bitmaps
        int hamming = 0;
        for (int r = 0; r < 16; ++r) {
            uint16_t diff = rows16[r] ^ tmpl.rows[r];
#if defined(__GNUC__) || defined(__clang__)
            hamming += __builtin_popcount(diff);
#else
            for (int b = 0; b < 16; ++b) {
                if (diff & (1 << b)) hamming++;
            }
#endif
        }

        // Aspect ratio penalty
        float arDiff = std::abs(ar - tmpl.ar);
        int arPenalty = static_cast<int>(arDiff * 20.0f);

        int totalScore = hamming + loopPenalty + arPenalty;
        if (totalScore < bestDist) {
            bestDist = totalScore;
            bestChar = tmpl.c;
        }
    }

    return bestChar;
}

std::string OcrEngine::recognizeGrayscale(const uint8_t* grayPixels, int width, int height) const {
    if (!grayPixels || width <= 0 || height <= 0) return "";

    auto binary = binarizeAdaptive(grayPixels, width, height);
    auto lines = segmentLines(binary.data(), width, height);

    std::string fullText;
    for (const auto& lineBox : lines) {
        std::vector<bool> spaceBefore;
        auto glyphs = segmentGlyphsInLine(binary.data(), width, height, lineBox, spaceBefore);

        std::string lineStr;
        for (size_t g = 0; g < glyphs.size(); ++g) {
            if (spaceBefore[g] && !lineStr.empty() && lineStr.back() != ' ') {
                lineStr.push_back(' ');
            }
            char c = classifyGlyph(binary.data(), width, glyphs[g]);
            lineStr.push_back(c);
        }

        if (!lineStr.empty()) {
            fullText += lineStr + "\n";
        }
    }

    return fullText;
}

std::vector<std::vector<uint8_t>> OcrEngine::extractImagesFromPdf(const std::vector<uint8_t>& pdfBytes) const {
    std::vector<std::vector<uint8_t>> images;
    const size_t totalSize = pdfBytes.size();
    if (totalSize < 50) return images;

    const char* data = reinterpret_cast<const char*>(pdfBytes.data());
    size_t pos = 0;

    while (pos < totalSize) {
        const char* streamStart = strstr(data + pos, "stream");
        if (!streamStart) break;

        size_t streamWordPos = streamStart - data;
        size_t dictEnd = streamWordPos;
        size_t dictStart = (dictEnd >= 600) ? (dictEnd - 600) : 0;
        std::string_view dictView(data + dictStart, dictEnd - dictStart);

        bool isImage = (dictView.find("/Image") != std::string_view::npos ||
                        dictView.find("/Subtype /Image") != std::string_view::npos);
        bool isJpeg = (dictView.find("/DCTDecode") != std::string_view::npos);
        bool isFlate = (dictView.find("/FlateDecode") != std::string_view::npos);

        const char* streamDataStart = streamStart + 6;
        if (*streamDataStart == '\r') streamDataStart++;
        if (*streamDataStart == '\n') streamDataStart++;

        const char* streamEnd = strstr(streamDataStart, "endstream");
        if (!streamEnd) break;

        size_t streamLen = streamEnd - streamDataStart;

        if (isImage && streamLen > 500) {
            if (isJpeg) {
                // Raw JPEG byte stream
                std::vector<uint8_t> jpegBytes(streamDataStart, streamDataStart + streamLen);
                images.push_back(std::move(jpegBytes));
            } else if (isFlate) {
                mz_ulong uncompLen = streamLen * 10 + 65536;
                std::vector<uint8_t> uncompressed(uncompLen);
                int status = mz_uncompress(uncompressed.data(), &uncompLen,
                                           reinterpret_cast<const unsigned char*>(streamDataStart), streamLen);
                if (status == MZ_OK) {
                    uncompressed.resize(uncompLen);
                    // Check if it looks like image data
                    if (uncompressed.size() >= 1000) {
                        images.push_back(std::move(uncompressed));
                    }
                }
            }
        }

        pos = (streamEnd - data) + 9;
    }

    return images;
}

std::string OcrEngine::extractAndRecognizePdf(const std::string& pdfPath, const std::vector<uint8_t>& pdfBytes) const {
    std::string fullOcrText;

    // Approach 1: Check if pdftoppm is available to render pages directly to images
    std::string tmpPrefix = "/tmp/bsc_render_" + std::to_string(std::rand());
    std::string renderCmd = "pdftoppm -r 150 -png -f 1 -l 10 \"" + pdfPath + "\" \"" + tmpPrefix + "\" > /dev/null 2>&1";
    int renderRet = std::system(renderCmd.c_str());

    if (renderRet == 0) {
        // Rendered pages: tmpPrefix-1.png, tmpPrefix-2.png...
        for (int page = 1; page <= 10; ++page) {
            std::string pageFile = tmpPrefix + "-" + std::to_string(page) + ".png";
            std::ifstream test(pageFile);
            if (!test.is_open()) {
                // Try format with leading zero: tmpPrefix-01.png
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s-%02d.png", tmpPrefix.c_str(), page);
                pageFile = buf;
                std::ifstream test2(pageFile);
                if (!test2.is_open()) break;
            }
            test.close();

            std::string pageText = recognizeImageFile(pageFile);
            fullOcrText += pageText + "\n";
            std::remove(pageFile.c_str());
        }
    }

    // Approach 2: If pdftoppm is not present or failed, extract embedded image streams directly
    if (fullOcrText.empty()) {
        auto embeddedImages = extractImagesFromPdf(pdfBytes);
        for (const auto& imgBytes : embeddedImages) {
            std::string pageText = recognizeImageMemory(imgBytes.data(), imgBytes.size());
            if (!pageText.empty()) {
                fullOcrText += pageText + "\n";
            }
        }
    }

    return postProcessFinancialText(fullOcrText);
}

std::string OcrEngine::postProcessFinancialText(const std::string& rawText) const {
    if (rawText.empty()) return "";

    std::vector<std::string> lines = core::Utils::split(rawText, '\n');
    std::string cleaned;

    for (auto& line : lines) {
        std::string s = core::Utils::trim(line);
        if (s.empty()) continue;

        // Contextual OCR correction for financial statements:
        // 1. Correct 'O' or 'o' to '0' and 'l'/'I' to '1' in numeric/currency contexts
        bool inNumber = false;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == 'R' && i + 1 < s.size() && s[i + 1] == '$') {
                inNumber = true;
            } else if (std::isdigit(static_cast<unsigned char>(s[i]))) {
                inNumber = true;
            } else if (s[i] == ' ' || s[i] == '\t') {
                inNumber = false;
            } else if (inNumber && (s[i] == 'O' || s[i] == 'o')) {
                s[i] = '0';
            } else if (inNumber && (s[i] == 'l' || s[i] == 'I')) {
                bool leftNum = (i > 0 && (std::isdigit(static_cast<unsigned char>(s[i - 1])) || s[i - 1] == ',' || s[i - 1] == '.'));
                bool rightNum = (i + 1 < s.size() && (std::isdigit(static_cast<unsigned char>(s[i + 1])) || s[i + 1] == ',' || s[i + 1] == '.'));
                if (leftNum || rightNum) {
                    s[i] = '1';
                }
            }
        }

        // 2. Correct currency symbols and spacing: R $ -> R$
        size_t rSpace = 0;
        while ((rSpace = s.find("R $", rSpace)) != std::string::npos) {
            s.replace(rSpace, 3, "R$");
            rSpace += 2;
        }

        // 3. Known bank naming corrections
        std::string up = core::Utils::toUpper(s);
        if (up.find("MERCADOPAGO") != std::string::npos || up.find("MERCADO PAGO") != std::string::npos) {
            // Keep verified casing
        }

        cleaned += s + "\n";
    }

    return cleaned;
}

} // namespace ofx::ocr
