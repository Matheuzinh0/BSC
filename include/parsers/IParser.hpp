/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "core/Types.hpp"
#include <memory>
#include <string>

namespace ofx::parsers {

class IParser {
public:
    virtual ~IParser() = default;
    virtual core::FileFormat getFormat() const = 0;
    virtual bool canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const = 0;
    virtual std::shared_ptr<core::Statement> parse(const std::string& filePath) const = 0;
};

} // namespace ofx::parsers
