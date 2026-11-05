/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "encoders/IEncoder.hpp"

namespace ofx::encoders {

class JsonEncoder : public IEncoder {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::JSON; }
    bool encode(const core::Statement& statement, const std::string& outputFilePath) const override;
    std::string encodeToString(const core::Statement& statement, bool pretty = true) const;
};

} // namespace ofx::encoders
