/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "parsers/PdfParser.hpp"
#include "core/Utils.hpp"
#include "ocr/OcrEngine.hpp"
#include "miniz/miniz.h"
#include <regex>
#include <iostream>
#include <cstring>

namespace ofx::parsers {

bool PdfParser::canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const {
    if (headerSize >= 4 && headerData[0] == '%' && headerData[1] == 'P' && headerData[2] == 'D' && headerData[3] == 'F') {
        return true;
    }
    return filePath.size() >= 4 && 
           core::Utils::toLower(filePath.substr(filePath.size() - 4)) == ".pdf";
}

std::string PdfParser::extractRawTextFromPdf(const std::vector<uint8_t>& pdfBytes) const {
    const size_t totalSize = pdfBytes.size();
    if (totalSize < 10) return "";

    const char* data = reinterpret_cast<const char*>(pdfBytes.data());
    size_t pos = 0;

    struct TextFragment {
        double x = 0.0;
        double y = 0.0;
        std::string text;
    };

    std::string fullText;

    while (pos < totalSize) {
        // Search for "stream" keyword
        const char* streamStart = strstr(data + pos, "stream");
        if (!streamStart) break;

        size_t streamWordPos = streamStart - data;
        
        // Find preceding dictionary "<<" before "stream"
        size_t dictEnd = streamWordPos;
        size_t dictStart = (dictEnd >= 600) ? (dictEnd - 600) : 0;
        std::string_view dictView(data + dictStart, dictEnd - dictStart);
        bool isFlate = (dictView.find("/FlateDecode") != std::string_view::npos);

        // Advance past "stream" and newline (\r\n or \n)
        const char* streamDataStart = streamStart + 6;
        if (*streamDataStart == '\r') streamDataStart++;
        if (*streamDataStart == '\n') streamDataStart++;

        // Search for "endstream"
        const char* streamEnd = strstr(streamDataStart, "endstream");
        if (!streamEnd) break;

        size_t streamLen = streamEnd - streamDataStart;
        if (streamLen > 0) {
            std::vector<uint8_t> uncompressed;
            if (isFlate) {
                // Decompress using miniz
                mz_ulong uncompLen = streamLen * 12 + 8192;
                uncompressed.resize(uncompLen);
                int status = mz_uncompress(uncompressed.data(), &uncompLen, 
                                           reinterpret_cast<const unsigned char*>(streamDataStart), streamLen);
                if (status == MZ_OK) {
                    uncompressed.resize(uncompLen);
                } else {
                    uncompLen = streamLen * 30 + 131072;
                    uncompressed.resize(uncompLen);
                    status = mz_uncompress(uncompressed.data(), &uncompLen, 
                                           reinterpret_cast<const unsigned char*>(streamDataStart), streamLen);
                    if (status == MZ_OK) {
                        uncompressed.resize(uncompLen);
                    } else {
                        uncompressed.clear();
                    }
                }
            } else {
                uncompressed.assign(streamDataStart, streamDataStart + streamLen);
            }

            if (!uncompressed.empty()) {
                const char* content = reinterpret_cast<const char*>(uncompressed.data());
                size_t cSize = uncompressed.size();
                size_t cPos = 0;

                std::vector<TextFragment> fragments;
                std::string streamFallback;

                double curX = 0.0, curY = 0.0;
                double lineX = 0.0, lineY = 0.0;
                double leading = 12.0;

                while (cPos < cSize) {
                    // Look for BT (Begin Text) operator
                    const char* bt = nullptr;
                    const char* bScan = content + cPos;
                    bool inParen = false;
                    while (bScan + 1 < content + cSize) {
                        if (*bScan == '\\' && inParen) { bScan += 2; continue; }
                        if (*bScan == '(') inParen = true;
                        else if (*bScan == ')') inParen = false;
                        else if (!inParen && bScan[0] == 'B' && bScan[1] == 'T') {
                            bool leftOk = (bScan == content || std::isspace(static_cast<unsigned char>(bScan[-1])));
                            bool rightOk = (bScan + 2 >= content + cSize || std::isspace(static_cast<unsigned char>(bScan[2])));
                            if (leftOk && rightOk) {
                                bt = bScan;
                                break;
                            }
                        }
                        bScan++;
                    }
                    if (!bt) break;

                    // Look for matching ET (End Text) operator
                    const char* et = nullptr;
                    const char* eScan = bt + 2;
                    inParen = false;
                    while (eScan + 1 < content + cSize) {
                        if (*eScan == '\\' && inParen) { eScan += 2; continue; }
                        if (*eScan == '(') inParen = true;
                        else if (*eScan == ')') inParen = false;
                        else if (!inParen && eScan[0] == 'E' && eScan[1] == 'T') {
                            bool leftOk = (eScan == content || std::isspace(static_cast<unsigned char>(eScan[-1])));
                            bool rightOk = (eScan + 2 >= content + cSize || std::isspace(static_cast<unsigned char>(eScan[2])));
                            if (leftOk && rightOk) {
                                et = eScan;
                                break;
                            }
                        }
                        eScan++;
                    }
                    if (!et) et = content + cSize;

                    // Parse inside BT ... ET
                    curX = 0.0; curY = 0.0;
                    lineX = 0.0; lineY = 0.0;
                    const char* p = bt + 2;

                    auto decodePdfString = [](std::string_view token) -> std::string {
                        std::string cleaned;
                        for (size_t i = 0; i < token.size(); ++i) {
                            if (token[i] == '\\' && i + 1 < token.size()) {
                                char next = token[++i];
                                if (next == 'n') cleaned.push_back('\n');
                                else if (next == 'r') cleaned.push_back('\r');
                                else if (next == 't') cleaned.push_back('\t');
                                else if (next == 'b') cleaned.push_back('\b');
                                else if (next == 'f') cleaned.push_back('\f');
                                else if (next >= '0' && next <= '7') {
                                    // Octal escape \ooo
                                    int octVal = next - '0';
                                    if (i + 1 < token.size() && token[i+1] >= '0' && token[i+1] <= '7') {
                                        octVal = octVal * 8 + (token[++i] - '0');
                                        if (i + 1 < token.size() && token[i+1] >= '0' && token[i+1] <= '7') {
                                            octVal = octVal * 8 + (token[++i] - '0');
                                        }
                                    }
                                    // If octVal is ISO-8859-1 / WinAnsi Latin accented character, convert to UTF-8
                                    if (octVal >= 160 && octVal <= 255) {
                                        cleaned.push_back(static_cast<char>(0xC0 | (octVal >> 6)));
                                        cleaned.push_back(static_cast<char>(0x80 | (octVal & 0x3F)));
                                    } else {
                                        cleaned.push_back(static_cast<char>(octVal));
                                    }
                                } else {
                                    cleaned.push_back(next);
                                }
                            } else {
                                cleaned.push_back(token[i]);
                            }
                        }
                        return cleaned;
                    };

                    auto decodeHex = [](std::string_view hexStr) -> std::string {
                        std::string decoded;
                        std::string curByte;
                        for (char c : hexStr) {
                            if (std::isspace(static_cast<unsigned char>(c))) continue;
                            curByte.push_back(c);
                            if (curByte.size() == 2) {
                                try {
                                    char byteVal = static_cast<char>(std::stoi(curByte, nullptr, 16));
                                    decoded.push_back(byteVal);
                                } catch (...) {}
                                curByte.clear();
                            }
                        }
                        if (curByte.size() == 1) {
                            curByte.push_back('0');
                            try {
                                char byteVal = static_cast<char>(std::stoi(curByte, nullptr, 16));
                                decoded.push_back(byteVal);
                            } catch (...) {}
                        }
                        // UTF-16BE detection
                        if (decoded.size() >= 2) {
                            if (static_cast<unsigned char>(decoded[0]) == 0xFE && static_cast<unsigned char>(decoded[1]) == 0xFF) {
                                std::string utf8;
                                for (size_t i = 2; i + 1 < decoded.size(); i += 2) {
                                    uint16_t ch = (static_cast<uint8_t>(decoded[i]) << 8) | static_cast<uint8_t>(decoded[i + 1]);
                                    if (ch >= 32 && ch < 127) utf8.push_back(static_cast<char>(ch));
                                    else if (ch >= 160 && ch <= 255) {
                                        utf8.push_back(static_cast<char>(0xC0 | (ch >> 6)));
                                        utf8.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
                                    } else utf8.push_back(' ');
                                }
                                return utf8;
                            } else if (decoded.size() % 2 == 0) {
                                bool allEvenZeros = true;
                                for (size_t i = 0; i < decoded.size(); i += 2) {
                                    if (decoded[i] != 0) { allEvenZeros = false; break; }
                                }
                                if (allEvenZeros) {
                                    std::string cleanAscii;
                                    for (size_t i = 1; i < decoded.size(); i += 2) {
                                        cleanAscii.push_back(decoded[i]);
                                    }
                                    return cleanAscii;
                                }
                            }
                        }
                        return decoded;
                    };

                    while (p < et) {
                        while (p < et && std::isspace(static_cast<unsigned char>(*p))) p++;
                        if (p >= et) break;

                        if (*p == '(') {
                            // Literal string (text)
                            const char* start = p + 1;
                            const char* s = start;
                            int nested = 1;
                            while (s < et && nested > 0) {
                                if (*s == '\\') { s += 2; continue; }
                                if (*s == '(') nested++;
                                else if (*s == ')') nested--;
                                s++;
                            }
                            std::string_view rawTok(start, (s - 1) - start);
                            std::string decoded = decodePdfString(rawTok);
                            p = s;

                            while (p < et && std::isspace(static_cast<unsigned char>(*p))) p++;
                            if (p + 2 <= et && p[0] == 'T' && p[1] == 'j') p += 2;
                            else if (p < et && *p == '\'') { lineY -= leading; curX = lineX; curY = lineY; p++; }

                            fragments.push_back({curX, curY, decoded});
                            streamFallback += decoded + " ";
                        } else if (*p == '<') {
                            if (p + 1 < et && p[1] == '<') {
                                p += 2;
                                while (p + 1 < et && !(p[0] == '>' && p[1] == '>')) p++;
                                if (p + 1 < et) p += 2;
                                continue;
                            }
                            const char* start = p + 1;
                            const char* s = start;
                            while (s < et && *s != '>') s++;
                            std::string_view rawHex(start, s - start);
                            std::string decoded = decodeHex(rawHex);
                            p = (s < et) ? s + 1 : et;

                            while (p < et && std::isspace(static_cast<unsigned char>(*p))) p++;
                            if (p + 2 <= et && p[0] == 'T' && p[1] == 'j') p += 2;

                            fragments.push_back({curX, curY, decoded});
                            streamFallback += decoded + " ";
                        } else if (*p == '[') {
                            // Array [(str1) -120 (str2)] TJ
                            p++;
                            std::string combinedArrayStr;
                            while (p < et && *p != ']') {
                                while (p < et && std::isspace(static_cast<unsigned char>(*p))) p++;
                                if (p >= et || *p == ']') break;

                                if (*p == '(') {
                                    const char* start = p + 1;
                                    const char* s = start;
                                    int nested = 1;
                                    while (s < et && nested > 0) {
                                        if (*s == '\\') { s += 2; continue; }
                                        if (*s == '(') nested++;
                                        else if (*s == ')') nested--;
                                        s++;
                                    }
                                    std::string_view rawTok(start, (s - 1) - start);
                                    combinedArrayStr += decodePdfString(rawTok);
                                    p = s;
                                } else if (*p == '<') {
                                    const char* start = p + 1;
                                    const char* s = start;
                                    while (s < et && *s != '>') s++;
                                    std::string_view rawHex(start, s - start);
                                    combinedArrayStr += decodeHex(rawHex);
                                    p = (s < et) ? s + 1 : et;
                                } else {
                                    // Number displacement
                                    const char* numStart = p;
                                    while (p < et && !std::isspace(static_cast<unsigned char>(*p)) && *p != '(' && *p != '<' && *p != ']') p++;
                                    try {
                                        double disp = std::stod(std::string(numStart, p - numStart));
                                        if (disp < -150.0) {
                                            combinedArrayStr += " ";
                                        }
                                    } catch (...) {}
                                }
                            }
                            if (p < et && *p == ']') p++;
                            while (p < et && std::isspace(static_cast<unsigned char>(*p))) p++;
                            if (p + 2 <= et && p[0] == 'T' && p[1] == 'J') p += 2;

                            fragments.push_back({curX, curY, combinedArrayStr});
                            streamFallback += combinedArrayStr + " ";
                        } else {
                            // Coordinate tokens or operators
                            const char* start = p;
                            while (p < et && !std::isspace(static_cast<unsigned char>(*p)) && *p != '(' && *p != '<' && *p != '[') p++;
                            std::string tok(start, p - start);

                            if (tok == "Td" || tok == "TD") {
                                const char* b = start - 1;
                                while (b > bt && std::isspace(static_cast<unsigned char>(*b))) b--;
                                const char* end2 = b + 1;
                                while (b > bt && !std::isspace(static_cast<unsigned char>(*b))) b--;
                                std::string sTy(b + 1, end2 - (b + 1));

                                while (b > bt && std::isspace(static_cast<unsigned char>(*b))) b--;
                                const char* end1 = b + 1;
                                while (b > bt && !std::isspace(static_cast<unsigned char>(*b))) b--;
                                std::string sTx(b + 1, end1 - (b + 1));

                                try {
                                    double tx = std::stod(sTx);
                                    double ty = std::stod(sTy);
                                    if (std::abs(ty) > 40.0) {
                                        lineX = tx;
                                        lineY = ty;
                                    } else {
                                        lineX += tx;
                                        lineY += ty;
                                    }
                                    curX = lineX;
                                    curY = lineY;
                                    if (tok == "TD") leading = (std::abs(ty) > 40.0) ? 12.0 : -ty;
                                } catch (...) {}
                            } else if (tok == "Tm") {
                                const char* b = start - 1;
                                while (b > bt && std::isspace(static_cast<unsigned char>(*b))) b--;
                                const char* endF = b + 1;
                                while (b > bt && !std::isspace(static_cast<unsigned char>(*b))) b--;
                                std::string sF(b + 1, endF - (b + 1));

                                while (b > bt && std::isspace(static_cast<unsigned char>(*b))) b--;
                                const char* endE = b + 1;
                                while (b > bt && !std::isspace(static_cast<unsigned char>(*b))) b--;
                                std::string sE(b + 1, endE - (b + 1));

                                try {
                                    double e = std::stod(sE);
                                    double f = std::stod(sF);
                                    lineX = e; lineY = f;
                                    curX = e; curY = f;
                                } catch (...) {}
                            } else if (tok == "T*") {
                                lineY -= leading;
                                curX = lineX;
                                curY = lineY;
                            }
                        }
                    }

                    streamFallback += "\n";
                    cPos = (et - content) + 2;
                }

                // 2D Visual Grid Reconstruction for this stream
                bool hasCoords = false;
                for (const auto& frag : fragments) {
                    if (std::abs(frag.x) > 0.001 || std::abs(frag.y) > 0.001) {
                        hasCoords = true;
                        break;
                    }
                }

                if (hasCoords && fragments.size() > 5) {
                    struct VisualLine {
                        double y = 0.0;
                        std::vector<TextFragment> items;
                    };
                    std::vector<VisualLine> lines;

                    for (const auto& frag : fragments) {
                        if (frag.text.empty()) continue;
                        bool added = false;
                        for (auto& vl : lines) {
                            if (std::abs(vl.y - frag.y) <= 3.5) {
                                vl.items.push_back(frag);
                                added = true;
                                break;
                            }
                        }
                        if (!added) {
                            VisualLine vl;
                            vl.y = frag.y;
                            vl.items.push_back(frag);
                            lines.push_back(std::move(vl));
                        }
                    }

                    // Sort lines top to bottom (Y descending)
                    std::sort(lines.begin(), lines.end(), [](const VisualLine& a, const VisualLine& b) {
                        return a.y > b.y;
                    });

                    std::string assembledStreamText;
                    for (auto& vl : lines) {
                        // Sort items left to right (X ascending)
                        std::sort(vl.items.begin(), vl.items.end(), [](const TextFragment& a, const TextFragment& b) {
                            return a.x < b.x;
                        });

                        for (size_t i = 0; i < vl.items.size(); ++i) {
                            assembledStreamText += vl.items[i].text;
                            if (i + 1 < vl.items.size()) {
                                assembledStreamText += " ";
                            }
                        }
                        assembledStreamText += "\n";
                    }

                    fullText += (assembledStreamText.size() > 10 ? assembledStreamText : streamFallback) + "\n";
                } else {
                    fullText += streamFallback + "\n";
                }
            }
        }

        pos = (streamEnd - data) + 9;
    }

    return fullText;
}

void PdfParser::parseBankMetadata(const std::string& text, core::Statement& statement) const {
    std::string upperText = core::Utils::toUpper(text);
    std::string headerArea = upperText.substr(0, std::min<size_t>(upperText.size(), 3000));

    struct BankMap {
        const char* key;
        const char* code;
        const char* name;
    };

    static const BankMap kBanks[] = {
        // Bancos Tradicionais e Multiplos
        {"BANCO DO BRASIL", "001", "Banco do Brasil S.A."},
        {"AUTOATENDIMENTO BB", "001", "Banco do Brasil S.A."},
        {"SANTANDER", "033", "Banco Santander (Brasil) S.A."},
        {"CAIXA ECONOMICA", "104", "Caixa Economica Federal"},
        {"CAIXA ECONÔMICA", "104", "Caixa Economica Federal"},
        {"CEF", "104", "Caixa Economica Federal"},
        {"BRADESCO", "237", "Banco Bradesco S.A."},
        {"ITAU", "341", "Itau Unibanco S.A."},
        {"ITAÚ", "341", "Itau Unibanco S.A."},
        {"PERSONNALITE", "341", "Itau Personnalite"},
        {"SAFRA", "422", "Banco Safra S.A."},
        {"BANRISUL", "041", "Banco do Estado do Rio Grande do Sul"},
        {"BANCO DO NORDESTE", "004", "Banco do Nordeste do Brasil"},
        {"BNB", "004", "Banco do Nordeste do Brasil"},
        {"BANCO DA AMAZONIA", "003", "Banco da Amazonia S.A."},
        {"BASA", "003", "Banco da Amazonia S.A."},
        {"BRB", "070", "BRB - Banco de Brasilia"},
        {"BANCO DE BRASILIA", "070", "BRB - Banco de Brasilia"},
        {"VOTORANTIM", "655", "Banco Votorantim S.A. (Banco BV)"},
        {"BANCO BV", "655", "Banco Votorantim S.A. (Banco BV)"},

        // Cooperativas de Credito
        {"SICOOB", "756", "Bancoob - Sicoob"},
        {"BANCOOB", "756", "Bancoob - Sicoob"},
        {"SICREDI", "748", "Banco Sicredi S.A."},
        {"UNICRED", "136", "Unicred do Brasil"},
        {"AILOS", "085", "Cooperativa Central Ailos"},
        {"VIACREDI", "085", "Viacredi - Ailos"},
        {"CRESOL", "133", "Cresol Confederacao"},

        // Bancos Digitais e Fintechs
        {"NUBANK", "260", "Nu Pagamentos S.A. - Nubank"},
        {"NU PAGAMENTOS", "260", "Nu Pagamentos S.A. - Nubank"},
        {"BANCO INTER", "077", "Banco Inter S.A."},
        {"INTER", "077", "Banco Inter S.A."},
        {"C6 BANK", "336", "Banco C6 S.A."},
        {"C6", "336", "Banco C6 S.A."},
        {"BTG PACTUAL", "208", "Banco BTG Pactual S.A."},
        {"BTG", "208", "Banco BTG Pactual S.A."},
        {"BANCO ORIGINAL", "212", "Banco Original S.A."},
        {"ORIGINAL", "212", "Banco Original S.A."},
        {"DAYCOVAL", "707", "Banco Daycoval S.A."},
        {"BANCO BMG", "318", "Banco BMG S.A."},
        {"BMG", "318", "Banco BMG S.A."},
        {"BANCO PAN", "623", "Banco Pan S.A."},
        {"PAN", "623", "Banco Pan S.A."},
        {"NEON", "536", "Neon Pagamentos S.A."},
        {"AGIBANK", "121", "Banco Agibank S.A."},
        {"DIGIO", "335", "Banco Digio S.A."},
        {"BS2", "218", "Banco BS2 S.A."},
        {"MASTER", "243", "Banco Master S.A."},
        {"SOFISA", "637", "Banco Sofisa S.A."},

        // Plataformas de Pagamento e Adquirencia
        {"MERCADO PAGO", "323", "Mercado Pago IP Ltda"},
        {"MERCADO LIVRE", "323", "Mercado Pago / Mercado Livre"},
        {"MERCADOLIVRE", "323", "Mercado Pago / Mercado Livre"},
        {"MERCADOPAGO", "323", "Mercado Pago IP Ltda"},
        {"MERCADO ENVIOS", "323", "Mercado Pago / Mercado Livre"},
        {"MERCADOLIBRE", "323", "Mercado Pago / Mercado Livre"},
        {"PAGBANK", "290", "PagBank - PagSeguro Internet S.A."},
        {"PAGSEGURO", "290", "PagBank - PagSeguro Internet S.A."},
        {"PICPAY", "380", "PicPay Servicos S.A."},
        {"STONE", "197", "Stone Pagamentos S.A."},
        {"ASAAS", "461", "Asaas Gestao Financeira"},
        {"CORA", "403", "Cora Sociedade de Credito Direto"},
        {"EFI", "364", "Efi S.A. (antiga Gerencianet)"},
        {"EFÍ", "364", "Efi S.A. (antiga Gerencianet)"},
        {"GERENCIANET", "364", "Efi S.A. (antiga Gerencianet)"},
        {"WOOVI", "513", "Woovi / OpenPix"},
        {"OPENPIX", "513", "Woovi / OpenPix"},
        {"EBANX", "172", "Ebanx Instituicao de Pagamento"},
        {"CELCOIN", "301", "Celcoin Instituicao de Pagamento"},
        {"RECARGAPAY", "301", "RecargaPay IP Ltda"},
        {"DLOCAL", "331", "Dlocal Brasil IP"},

        // Investimentos e Globais
        {"XP INVESTIMENTOS", "102", "XP Investimentos CCTVM S.A."},
        {"XP", "102", "XP Investimentos CCTVM S.A."},
        {"GENIAL", "125", "Genial Investimentos"},
        {"MODALMAIS", "746", "Banco Modal / Modalmais"},
        {"MODAL", "746", "Banco Modal / Modalmais"},
        {"AVENUE", "888", "Avenue Securities"},
        {"NOMAD", "889", "Nomad Fintech"},
        {"WISE", "890", "Wise Brasil CVC"},
        {"TRANSFERWISE", "890", "Wise Brasil CVC"},
        {"MERCADO BITCOIN", "891", "Mercado Bitcoin"},
        {"BINANCE", "892", "Binance Brasil"}
    };

    auto matchesBankKey = [](const std::string& haystack, const std::string& key) -> bool {
        size_t pos = 0;
        while ((pos = haystack.find(key, pos)) != std::string::npos) {
            bool leftOk = (pos == 0 || !std::isalnum(static_cast<unsigned char>(haystack[pos - 1])));
            size_t endPos = pos + key.size();
            bool rightOk = (endPos == haystack.size() || !std::isalnum(static_cast<unsigned char>(haystack[endPos])));
            if (leftOk && rightOk) {
                return true;
            }
            pos = endPos;
        }
        return false;
    };

    bool bankFound = false;
    for (const auto& b : kBanks) {
        if (matchesBankKey(headerArea, b.key)) {
            statement.account.bankId = b.code;
            statement.account.bankName = b.name;
            bankFound = true;
            break;
        }
    }

    if (!bankFound) {
        for (const auto& b : kBanks) {
            if (matchesBankKey(upperText, b.key)) {
                statement.account.bankId = b.code;
                statement.account.bankName = b.name;
                bankFound = true;
                break;
            }
        }
    }

    if (!bankFound) {
        std::regex dynBankRegex(R"((?:Banco|Institui[cç][aã]o|Cooperativa)[\s\:\-]+([A-Za-z0-9\s]{3,30}))", std::regex::icase);
        std::smatch match;
        if (std::regex_search(text, match, dynBankRegex)) {
            statement.account.bankName = core::Utils::trim(match[1].str());
            statement.account.bankId = "999";
        } else {
            statement.account.bankId = "999";
            statement.account.bankName = "Extrato Bancario Universal";
        }
    }

    // Extract Agência and Conta
    std::regex agRegex(R"((?:Ag[êe]ncia|Ag\.?|Prefix|Prefixo|Ponto de Atendimento|PA|Cooperativa)[\s\:\-]*([0-9]{3,5}(?:-[0-9X])?))", std::regex::icase);
    std::smatch agMatch;
    if (std::regex_search(text, agMatch, agRegex)) {
        statement.account.branchId = agMatch[1].str();
    }

    std::regex ccRegex(R"((?:Conta(?:\s+Corrente|\s+de\s+Pagamento|\s+Poupan[cç]a)?|C\/C|Cta\.?|N[oº]\.?\s*Conta)[\s\:\-]*([0-9\.\-]{4,18}))", std::regex::icase);
    std::smatch ccMatch;
    if (std::regex_search(text, ccMatch, ccRegex)) {
        statement.account.accountId = ccMatch[1].str();
    }

    if (statement.account.accountId.empty()) statement.account.accountId = "000000";
    if (statement.account.branchId.empty()) statement.account.branchId = "0001";
    statement.account.accountType = "CHECKING";
}

void PdfParser::parseTransactionsFromText(const std::string& text, core::Statement& statement) const {
    std::vector<std::string> lines = core::Utils::split(text, '\n');

    std::regex dateRegex(R"(\b(?:\d{1,2}(?:\s+de\s+|\s*[\/\-\.]\s*|\s+)(?:janeiro|fevereiro|março|marco|abril|maio|junho|julho|agosto|setembro|outubro|novembro|dezembro|jan|fev|mar|abr|mai|jun|jul|ago|set|out|nov|dez|oct|dec)(?:\.?)?(?:(?:\s+de\s+|\s*[\/\-\.]\s*|\s+)\d{2,4})?|\d{1,2}[\/\-\.]\d{1,2}(?:[\/\-\.]\d{2,4})?|\d{4}[\/\-]\d{1,2}[\/\-]\d{1,2})\b)", std::regex::icase);
    std::regex amtRegex(R"((?:[\+\-]\s*(?:R\$\s*)?|(?:R\$\s*)[\+\-]\s*|(?:R\$\s*))?\(?\d{1,3}(?:\.\d{3})*,\d{2}\)?(?:\s*[CD\-])?(?!\/|\d)|(?:[\+\-]\s*(?:R\$\s*)?|(?:R\$\s*)[\+\-]\s*|(?:R\$\s*))?\(?\d{1,3}(?:,\d{3})*\.\d{2}\)?(?:\s*[CD\-])?(?!\/|\d))");

    int currentYear = 2024;
    std::regex yearScan(R"(\b(20[123][0-9])\b)");
    std::smatch yMatch;
    if (std::regex_search(text, yMatch, yearScan)) {
        try { currentYear = std::stoi(yMatch[1].str()); } catch (...) {}
    }

    bool creditFirst = false;
    std::string pendingDate;
    std::string pendingDesc;

    for (const auto& rawLine : lines) {
        std::string line = core::Utils::trim(rawLine);
        if (line.size() < 3) continue;

        std::string upLine = core::Utils::toUpper(line);

        // Detect column order from table header
        if (upLine.find("DATA") != std::string::npos || upLine.find("ENTRADA") != std::string::npos || 
            upLine.find("CREDITO") != std::string::npos || upLine.find("DEBITO") != std::string::npos) {
            size_t posCred = std::string::npos;
            size_t posDeb = std::string::npos;

            if (upLine.find("ENTRADA") != std::string::npos) posCred = upLine.find("ENTRADA");
            else if (upLine.find("CREDITO") != std::string::npos) posCred = upLine.find("CREDITO");
            else if (upLine.find("CRÉDITO") != std::string::npos) posCred = upLine.find("CRÉDITO");

            if (upLine.find("SAIDA") != std::string::npos) posDeb = upLine.find("SAIDA");
            else if (upLine.find("SAÍDA") != std::string::npos) posDeb = upLine.find("SAÍDA");
            else if (upLine.find("DEBITO") != std::string::npos) posDeb = upLine.find("DEBITO");
            else if (upLine.find("DÉBITO") != std::string::npos) posDeb = upLine.find("DÉBITO");

            if (posCred != std::string::npos && posDeb != std::string::npos) {
                creditFirst = (posCred < posDeb);
            }
        }

        // Balances detection
        if (upLine.find("SALDO ANTERIOR") != std::string::npos || 
            upLine.find("SALDO INICIAL") != std::string::npos ||
            upLine.find("SALDO DEVEDOR ANTERIOR") != std::string::npos) {
            auto it = std::sregex_iterator(line.begin(), line.end(), amtRegex);
            if (it != std::sregex_iterator()) {
                statement.initialBalance = core::Utils::parseAmount(it->str());
                statement.hasInitialBalance = true;
            }
            continue;
        }

        if (upLine.find("SALDO FINAL") != std::string::npos || 
            upLine.find("SALDO ATUAL") != std::string::npos ||
            upLine.find("SALDO DISPONIVEL") != std::string::npos ||
            upLine.find("SALDO DISPONÍVEL") != std::string::npos) {
            auto it = std::sregex_iterator(line.begin(), line.end(), amtRegex);
            if (it != std::sregex_iterator()) {
                statement.finalBalance = core::Utils::parseAmount(it->str());
                statement.hasFinalBalance = true;
            }
            continue;
        }

        // Ignore common headers, summaries and footers
        if (upLine.find("DATA") != std::string::npos && 
            (upLine.find("HISTORICO") != std::string::npos || upLine.find("HISTÓRICO") != std::string::npos || upLine.find("VALOR") != std::string::npos)) {
            continue;
        }
        if (upLine.find("TOTAL CREDITOS") != std::string::npos || 
            upLine.find("TOTAL DEBITOS") != std::string::npos ||
            upLine.find("TOTAL DÉBITOS") != std::string::npos ||
            upLine.find("FLUXO LIQUIDO") != std::string::npos ||
            upLine.find("FLUXO LÍQUIDO") != std::string::npos ||
            upLine.find("RESUMO DO PERIODO") != std::string::npos ||
            upLine.find("RESUMO DO PERÍODO") != std::string::npos) {
            continue;
        }
        if (upLine.find("PERIODO:") != std::string::npos || upLine.find("PERÍODO:") != std::string::npos ||
            (upLine.find("PERIODO") != std::string::npos && upLine.find(" A ") != std::string::npos) ||
            (upLine.find("PERÍODO") != std::string::npos && upLine.find(" A ") != std::string::npos)) {
            continue;
        }
        if (upLine.find("PAGINA ") != std::string::npos || upLine.find("PÁGINA ") != std::string::npos) {
            continue;
        }
        if (upLine.find("OUVIDORIA") != std::string::npos || upLine.find("SAC 0800") != std::string::npos) {
            continue;
        }

        // Check for Date match
        std::smatch dMatch;
        bool hasDate = std::regex_search(line, dMatch, dateRegex);

        // Check for Amount matches
        auto amtBegin = std::sregex_iterator(line.begin(), line.end(), amtRegex);
        auto amtEnd = std::sregex_iterator();
        std::vector<std::smatch> amtMatches;
        for (auto it = amtBegin; it != amtEnd; ++it) {
            amtMatches.push_back(*it);
        }

        if (hasDate && !amtMatches.empty()) {
            pendingDate.clear();
            pendingDesc.clear();

            core::Transaction tx;
            std::string dateStr = dMatch[0].str();
            tx.date = core::Utils::normalizeDate(dateStr);
            if (tx.date.size() == 5) {
                tx.date = std::to_string(currentYear) + "-" + dateStr.substr(3, 2) + "-" + dateStr.substr(0, 2);
            }

            // Determine amount and optional running balance
            if (amtMatches.size() == 1) {
                tx.amount = core::Utils::parseAmount(amtMatches[0].str());
            } else if (amtMatches.size() == 2) {
                tx.amount = core::Utils::parseAmount(amtMatches[0].str());
                tx.runningBalance = core::Utils::parseAmount(amtMatches[1].str());
            } else if (amtMatches.size() >= 3) {
                double val1 = std::abs(core::Utils::parseAmount(amtMatches[0].str()));
                double val2 = std::abs(core::Utils::parseAmount(amtMatches[1].str()));
                if (creditFirst) {
                    if (val1 > 0.0) tx.amount = val1;
                    else if (val2 > 0.0) tx.amount = -val2;
                } else {
                    if (val2 > 0.0) tx.amount = val2;
                    else if (val1 > 0.0) tx.amount = -val1;
                }
                tx.runningBalance = core::Utils::parseAmount(amtMatches.back().str());
            }

            // Contextual sign detection if amount is positive but description indicates debit
            if (tx.amount > 0.0) {
                std::string descCheck = core::Utils::toUpper(line);
                bool hasDebitWord = (descCheck.find("PAGTO") != std::string::npos ||
                                     descCheck.find("PAGAMENTO") != std::string::npos ||
                                     descCheck.find("TARIFA") != std::string::npos ||
                                     descCheck.find("SAQUE") != std::string::npos ||
                                     descCheck.find("COMPRA") != std::string::npos ||
                                     descCheck.find("DEBITO") != std::string::npos ||
                                     descCheck.find("DÉBITO") != std::string::npos ||
                                     descCheck.find("ENVIO") != std::string::npos ||
                                     descCheck.find("ENVIADO") != std::string::npos ||
                                     descCheck.find("IOF") != std::string::npos);
                if (hasDebitWord && descCheck.find("ESTORNO") == std::string::npos &&
                    descCheck.find("RECEB") == std::string::npos) {
                    tx.amount = -tx.amount;
                }
            }

            if (tx.amount > 0.0) tx.type = core::TransactionType::CREDIT;
            else if (tx.amount < 0.0) tx.type = core::TransactionType::DEBIT;

            // Extract Description by masking date and amounts
            std::string textOnly = line;
            textOnly.replace(dMatch.position(), dMatch.length(), std::string(dMatch.length(), ' '));
            for (const auto& m : amtMatches) {
                textOnly.replace(m.position(), m.length(), std::string(m.length(), ' '));
            }

            std::string cleanDesc;
            bool inSpace = false;
            for (char c : textOnly) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    if (!inSpace) { cleanDesc.push_back(' '); inSpace = true; }
                } else {
                    cleanDesc.push_back(c);
                    inSpace = false;
                }
            }
            cleanDesc = core::Utils::trim(cleanDesc);

            // Document number extraction
            auto tokens = core::Utils::split(cleanDesc, ' ');
            if (tokens.size() > 1) {
                std::string firstTok = tokens[0];
                bool isDocLike = (firstTok.size() >= 3 && firstTok.size() <= 12 &&
                                  (std::all_of(firstTok.begin(), firstTok.end(), [](char c){ return std::isdigit(c); }) ||
                                   firstTok.rfind("DOC", 0) == 0));
                if (isDocLike) {
                    tx.documentNumber = firstTok;
                    cleanDesc = cleanDesc.substr(firstTok.size());
                    cleanDesc = core::Utils::trim(cleanDesc);
                }
            }

            tx.description = cleanDesc;

            if (!tx.date.empty() && tx.amount != 0.0) {
                statement.transactions.push_back(std::move(tx));
            }
        } else if (hasDate && amtMatches.empty()) {
            // Stacked layout: Date on its own line
            pendingDate = core::Utils::normalizeDate(dMatch[0].str());
            if (pendingDate.size() == 5) {
                pendingDate = std::to_string(currentYear) + "-" + pendingDate.substr(3, 2) + "-" + pendingDate.substr(0, 2);
            }
            std::string textOnly = line;
            textOnly.replace(dMatch.position(), dMatch.length(), std::string(dMatch.length(), ' '));
            pendingDesc = core::Utils::trim(textOnly);
        } else if (!hasDate && !amtMatches.empty() && !pendingDate.empty()) {
            // Stacked layout completion: Amount line after pending date
            core::Transaction tx;
            tx.date = pendingDate;
            tx.amount = core::Utils::parseAmount(amtMatches[0].str());
            if (tx.amount > 0.0) tx.type = core::TransactionType::CREDIT;
            else if (tx.amount < 0.0) tx.type = core::TransactionType::DEBIT;

            std::string textOnly = line;
            for (const auto& m : amtMatches) {
                textOnly.replace(m.position(), m.length(), std::string(m.length(), ' '));
            }
            std::string extraDesc = core::Utils::trim(textOnly);
            if (!extraDesc.empty()) {
                if (!pendingDesc.empty()) pendingDesc += " ";
                pendingDesc += extraDesc;
            }
            tx.description = pendingDesc;

            statement.transactions.push_back(std::move(tx));
            pendingDate.clear();
            pendingDesc.clear();
        } else if (!hasDate && amtMatches.empty()) {
            // Continuation line
            if (!pendingDate.empty()) {
                if (!pendingDesc.empty()) pendingDesc += " ";
                pendingDesc += line;
            } else if (!statement.transactions.empty()) {
                if (line.size() > 2 && line.size() < 140 &&
                    upLine.find("TOTAL") == std::string::npos &&
                    upLine.find("EXTRATO") == std::string::npos &&
                    upLine.find("BANCO") == std::string::npos &&
                    upLine.find("AGENCIA") == std::string::npos &&
                    upLine.find("CONTA") == std::string::npos) {
                    statement.transactions.back().description += " " + line;
                }
            }
        }
    }
}

std::shared_ptr<core::Statement> PdfParser::parseFromText(const std::string& text) const {
    auto statement = std::make_shared<core::Statement>();
    statement->sourceFormat = core::FileFormat::PDF;

    parseBankMetadata(text, *statement);
    parseTransactionsFromText(text, *statement);
    statement->computeStatistics();

    return statement;
}

std::shared_ptr<core::Statement> PdfParser::parse(const std::string& filePath) const {
    std::vector<uint8_t> bytes = core::Utils::readFileBinary(filePath);
    if (bytes.empty()) {
        auto statement = std::make_shared<core::Statement>();
        statement->sourceFilePath = filePath;
        statement->sourceFormat = core::FileFormat::PDF;
        return statement;
    }

    // Step 1: Attempt ultra-fast vector text stream extraction
    std::string text = extractRawTextFromPdf(bytes);
    auto statement = parseFromText(text);
    statement->sourceFilePath = filePath;

    // Step 2: If no transactions detected (scanned PDF, raster image, or missing text streams), trigger OCR pipeline
    if (statement->transactions.empty()) {
        std::string ocrText = ocr::OcrEngine::instance().extractAndRecognizePdf(filePath, bytes);
        if (!ocrText.empty()) {
            auto ocrStatement = parseFromText(ocrText);
            ocrStatement->sourceFilePath = filePath;
            if (!ocrStatement->transactions.empty()) {
                return ocrStatement;
            }
        }
    }

    return statement;
}

} // namespace ofx::parsers
