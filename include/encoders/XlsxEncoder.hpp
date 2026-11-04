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

class XlsxEncoder : public IEncoder {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::XLSX; }
    bool encode(const core::Statement& statement, const std::string& outputFilePath) const override;

private:
    std::string generateSheetXml(const core::Statement& statement) const;
    std::string generateStylesXml() const;
};

} // namespace ofx::encoders
