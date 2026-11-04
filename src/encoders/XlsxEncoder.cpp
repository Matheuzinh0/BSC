/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "encoders/XlsxEncoder.hpp"
#include "core/Utils.hpp"
#include "miniz/miniz.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace ofx::encoders {

static std::string xmlEscape(std::string_view text) {
    std::string res;
    res.reserve(text.size() + 16);
    for (char c : text) {
        switch (c) {
            case '&': res += "&amp;"; break;
            case '<': res += "&lt;"; break;
            case '>': res += "&gt;"; break;
            case '"': res += "&quot;"; break;
            case '\'': res += "&apos;"; break;
            default: res.push_back(c); break;
        }
    }
    return res;
}

std::string XlsxEncoder::generateStylesXml() const {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
           "  <numFmts count=\"1\">\n"
           "    <numFmt numFmtId=\"164\" formatCode=\"#,##0.00\"/>\n"
           "  </numFmts>\n"
           "  <fonts count=\"4\">\n"
           "    <font><name val=\"Calibri\"/><sz val=\"11\"/></font>\n" // 0: Normal
           "    <font><name val=\"Calibri\"/><sz val=\"11\"/><b/><color rgb=\"FFFFFFFF\"/></font>\n" // 1: Header bold white
           "    <font><name val=\"Calibri\"/><sz val=\"11\"/><color rgb=\"FF10B981\"/></font>\n" // 2: Green credit
           "    <font><name val=\"Calibri\"/><sz val=\"11\"/><color rgb=\"FFEF4444\"/></font>\n" // 3: Red debit
           "  </fonts>\n"
           "  <fills count=\"3\">\n"
           "    <fill><patternFill patternType=\"none\"/></fill>\n"
           "    <fill><patternFill patternType=\"gray125\"/></fill>\n"
           "    <fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF1E293B\"/></patternFill></fill>\n" // 2: Dark slate header
           "  </fills>\n"
           "  <borders count=\"1\">\n"
           "    <border><left/><right/><top/><bottom/><diagonal/></border>\n"
           "  </borders>\n"
           "  <cellStyleXfs count=\"1\">\n"
           "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n"
           "  </cellStyleXfs>\n"
           "  <cellXfs count=\"5\">\n"
           "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>\n" // 0: Normal
           "    <xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\"/>\n" // 1: Header
           "    <xf numFmtId=\"164\" fontId=\"2\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\" applyFont=\"1\"/>\n" // 2: Credit #,##0.00
           "    <xf numFmtId=\"164\" fontId=\"3\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\" applyFont=\"1\"/>\n" // 3: Debit #,##0.00
           "    <xf numFmtId=\"164\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>\n" // 4: Neutral numeric #,##0.00
           "  </cellXfs>\n"
           "</styleSheet>";
}

std::string XlsxEncoder::generateSheetXml(const core::Statement& statement) const {
    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    ss << "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n";
    
    // Column widths
    ss << "  <cols>\n";
    ss << "    <col min=\"1\" max=\"1\" width=\"14\" customWidth=\"1\"/>\n"; // Data
    ss << "    <col min=\"2\" max=\"2\" width=\"35\" customWidth=\"1\"/>\n"; // Descricao
    ss << "    <col min=\"3\" max=\"3\" width=\"18\" customWidth=\"1\"/>\n"; // Documento
    ss << "    <col min=\"4\" max=\"4\" width=\"12\" customWidth=\"1\"/>\n"; // Tipo
    ss << "    <col min=\"5\" max=\"5\" width=\"18\" customWidth=\"1\"/>\n"; // Valor
    ss << "    <col min=\"6\" max=\"6\" width=\"18\" customWidth=\"1\"/>\n"; // Saldo
    ss << "    <col min=\"7\" max=\"7\" width=\"32\" customWidth=\"1\"/>\n"; // Hash
    ss << "  </cols>\n";

    ss << "  <sheetData>\n";

    // Row 1: Header
    ss << "    <row r=\"1\">\n";
    ss << "      <c r=\"A1\" t=\"inlineStr\" s=\"1\"><is><t>Data</t></is></c>\n";
    ss << "      <c r=\"B1\" t=\"inlineStr\" s=\"1\"><is><t>Descricao</t></is></c>\n";
    ss << "      <c r=\"C1\" t=\"inlineStr\" s=\"1\"><is><t>Documento</t></is></c>\n";
    ss << "      <c r=\"D1\" t=\"inlineStr\" s=\"1\"><is><t>Tipo</t></is></c>\n";
    ss << "      <c r=\"E1\" t=\"inlineStr\" s=\"1\"><is><t>Valor</t></is></c>\n";
    ss << "      <c r=\"F1\" t=\"inlineStr\" s=\"1\"><is><t>Saldo</t></is></c>\n";
    ss << "      <c r=\"G1\" t=\"inlineStr\" s=\"1\"><is><t>Hash SHA-256</t></is></c>\n";
    ss << "    </row>\n";

    int rowIdx = 2;
    for (const auto& tx : statement.transactions) {
        ss << "    <row r=\"" << rowIdx << "\">\n";
        
        // A: Date
        ss << "      <c r=\"A" << rowIdx << "\" t=\"inlineStr\"><is><t>" << xmlEscape(tx.date) << "</t></is></c>\n";
        // B: Description
        ss << "      <c r=\"B" << rowIdx << "\" t=\"inlineStr\"><is><t>" << xmlEscape(tx.description) << "</t></is></c>\n";
        // C: Document
        ss << "      <c r=\"C" << rowIdx << "\" t=\"inlineStr\"><is><t>" << xmlEscape(tx.documentNumber) << "</t></is></c>\n";
        // D: Tipo
        ss << "      <c r=\"D" << rowIdx << "\" t=\"inlineStr\"><is><t>" << (tx.isCredit() ? "CREDITO" : "DEBITO") << "</t></is></c>\n";
        // E: Valor (style 2 for credit green, 3 for debit red)
        int valStyle = tx.isCredit() ? 2 : 3;
        ss << "      <c r=\"E" << rowIdx << "\" s=\"" << valStyle << "\"><v>" << std::fixed << std::setprecision(2) << tx.amount << "</v></c>\n";
        // F: Saldo
        if (tx.runningBalance.has_value()) {
            ss << "      <c r=\"F" << rowIdx << "\" s=\"4\"><v>" << std::fixed << std::setprecision(2) << tx.runningBalance.value() << "</v></c>\n";
        }
        // G: Hash
        ss << "      <c r=\"G" << rowIdx << "\" t=\"inlineStr\"><is><t>" << xmlEscape(tx.hashSha256) << "</t></is></c>\n";

        ss << "    </row>\n";
        rowIdx++;
    }

    // Summary Row
    ss << "    <row r=\"" << rowIdx << "\">\n";
    ss << "      <c r=\"A" << rowIdx << "\" t=\"inlineStr\" s=\"1\"><is><t>TOTAL</t></is></c>\n";
    ss << "      <c r=\"B" << rowIdx << "\" t=\"inlineStr\" s=\"1\"><is><t>Fluxo Líquido</t></is></c>\n";
    ss << "      <c r=\"C" << rowIdx << "\" s=\"1\"/>\n";
    ss << "      <c r=\"D" << rowIdx << "\" s=\"1\"/>\n";
    int summaryStyle = (statement.netFlow >= 0.0) ? 2 : 3;
    ss << "      <c r=\"E" << rowIdx << "\" s=\"" << summaryStyle << "\"><v>" << std::fixed << std::setprecision(2) << statement.netFlow << "</v></c>\n";
    ss << "      <c r=\"F" << rowIdx << "\" s=\"1\"/>\n";
    ss << "      <c r=\"G" << rowIdx << "\" s=\"1\"/>\n";
    ss << "    </row>\n";

    ss << "  </sheetData>\n";
    ss << "</worksheet>";

    return ss.str();
}

bool XlsxEncoder::encode(const core::Statement& statement, const std::string& outputFilePath) const {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_file(&zip, outputFilePath.c_str(), 0)) {
        return false;
    }

    // 1. [Content_Types].xml
    const char* contentTypesXml = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
        "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
        "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
        "  <Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
        "  <Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
        "  <Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n"
        "</Types>";
    mz_zip_writer_add_mem(&zip, "[Content_Types].xml", contentTypesXml, strlen(contentTypesXml), MZ_BEST_COMPRESSION);

    // 2. _rels/.rels
    const char* relsXml = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
        "</Relationships>";
    mz_zip_writer_add_mem(&zip, "_rels/.rels", relsXml, strlen(relsXml), MZ_BEST_COMPRESSION);

    // 3. xl/_rels/workbook.xml.rels
    const char* wbRelsXml = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
        "  <Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n"
        "</Relationships>";
    mz_zip_writer_add_mem(&zip, "xl/_rels/workbook.xml.rels", wbRelsXml, strlen(wbRelsXml), MZ_BEST_COMPRESSION);

    // 4. xl/workbook.xml
    const char* wbXml = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
        "  <sheets>\n"
        "    <sheet name=\"Extrato\" sheetId=\"1\" r:id=\"rId1\"/>\n"
        "  </sheets>\n"
        "</workbook>";
    mz_zip_writer_add_mem(&zip, "xl/workbook.xml", wbXml, strlen(wbXml), MZ_BEST_COMPRESSION);

    // 5. xl/styles.xml
    std::string stylesXml = generateStylesXml();
    mz_zip_writer_add_mem(&zip, "xl/styles.xml", stylesXml.data(), stylesXml.size(), MZ_BEST_COMPRESSION);

    // 6. xl/worksheets/sheet1.xml
    std::string sheetXml = generateSheetXml(statement);
    mz_zip_writer_add_mem(&zip, "xl/worksheets/sheet1.xml", sheetXml.data(), sheetXml.size(), MZ_BEST_COMPRESSION);

    mz_bool finalizeStatus = mz_zip_writer_finalize_archive(&zip);
    mz_bool endStatus = mz_zip_writer_end(&zip);

    return finalizeStatus && endStatus;
}

} // namespace ofx::encoders
