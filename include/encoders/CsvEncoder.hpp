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

class CsvEncoder : public IEncoder {
public:
    explicit CsvEncoder(char delimiter = ';') : m_delimiter(delimiter) {}
    core::FileFormat getFormat() const override { return core::FileFormat::CSV; }
    bool encode(const core::Statement& statement, const std::string& outputFilePath) const override;

private:
    char m_delimiter;
    std::string escapeCell(std::string_view cell) const;
};

} // namespace ofx::encoders
