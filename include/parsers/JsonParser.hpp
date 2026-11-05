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

class JsonParser : public IParser {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::JSON; }
    bool canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const override;
    std::shared_ptr<core::Statement> parse(const std::string& filePath) const override;
    std::shared_ptr<core::Statement> parseFromString(const std::string& jsonContent) const;
};

} // namespace ofx::parsers
