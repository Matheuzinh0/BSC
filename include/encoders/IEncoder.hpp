/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "core/Types.hpp"
#include <string>

namespace ofx::encoders {

class IEncoder {
public:
    virtual ~IEncoder() = default;
    virtual core::FileFormat getFormat() const = 0;
    virtual bool encode(const core::Statement& statement, const std::string& outputFilePath) const = 0;
};

} // namespace ofx::encoders
