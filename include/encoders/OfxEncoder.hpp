/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "encoders/IEncoder.hpp"

namespace ofx::encoders {

class OfxEncoder : public IEncoder {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::OFX; }
    bool encode(const core::Statement& statement, const std::string& outputFilePath) const override;
};

} // namespace ofx::encoders
