/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "parsers/IParser.hpp"

namespace ofx::parsers {

class QifParser : public IParser {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::QIF; }
    bool canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const override;
    std::shared_ptr<core::Statement> parse(const std::string& filePath) const override;
};

} // namespace ofx::parsers
