# OmniStatement Studio (v2.5)
### Motor Universal de Conversão de Extratos Bancários em Alta Vazão (100% Modern C++20)

> **Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação, Departamento de Eletrônica e Sistemas, CTG-UFPE.**

Software financeiro de engenharia de telecomunicações e software de alto rendimento, projetado para processar **$N$ lotes de extratos bancários** de forma ultra-rápida, segura e com baixo consumo de memória, rodando perfeitamente tanto em hardware moderno quanto em processadores e placas gráficas legadas (Intel Core 2 Duo, Celeron, Atom, OpenGL 2.1 / GLSL 1.20, < 15 MB de RAM).

---

## 🚀 Matriz Universal de Conversão (Qualquer para Qualquer)

O software converte de forma bi-direcional e permutável entre todos os formatos bancários e corporativos:

| De / Para | PDF | OFX | CSV | XLSX | QIF |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **PDF** | 🔄 | ✅ | ✅ | ✅ | ✅ |
| **OFX** | ✅ | 🔄 | ✅ | ✅ | ✅ |
| **CSV** | ✅ | ✅ | 🔄 | ✅ | ✅ |
| **XLSX**| ✅ | ✅ | ✅ | 🔄 | ✅ |
| **QIF** | ✅ | ✅ | ✅ | ✅ | 🔄 |

---

## 🏛️ Premissas de Engenharia de Software e Telecomunicações

### 1. Engenharia de Telecomunicações & Taxa de Transferência
- **Arquitetura de Pipeline Multiplexado**: Processamento em estágios (Demux/Framing $\rightarrow$ Tokenização $\rightarrow$ Normalização Canônica $\rightarrow$ Roteamento $\rightarrow$ Serialização/Mux).
- **Worker Pool Adaptativo (`core::ThreadPool`)**: Escala automaticamente com a concorrência de hardware (`std::thread::hardware_concurrency()`). Em CPUs legadas de 2 núcleos, evita starvation e trocas de contexto desnecessárias.
- **Eficiência e Zero-Copy**: Utilização de `std::string_view`, buffers reutilizados e descompressão in-memory sem re-alocações.
- **Medição de Vazão em Tempo Real**: Telemetria contínua exibindo taxa de processamento em Transações/segundo (Tx/s) e Megabytes/segundo (MB/s).

### 2. Segurança da Informação & Integridade
- **100% Air-Gapped & Local**: Nenhuma conexão com redes externas, zero telemetria ou vazamento de dados bancários confidenciais.
- **Impressão Digital Criptográfica (SHA-256)**: Cada transação e cada extrato consolidado recebem um hash SHA-256 para não-repúdio, auditoria e desduplicação automática.
- **Verificação de Paridade de Livro-Razão (Ledger Balance Parity)**: Checagem contínua se o Saldo Final equivale ao Saldo Inicial acrescido do somatório das transações ($\text{Saldo}_{f} = \text{Saldo}_{i} + \sum \text{Tx}$).

---

## 💻 Requisitos e Compilação

### Dependências
- Compilador C++ moderno compatível com C++20 (`g++` 10+ ou `clang++` 11+)
- `cmake` (3.20+)
- `libglfw3-dev` e bibliotecas X11/OpenGL

### Compilação Rápida
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Executar a Bateria de Testes Automatizados
```bash
ctest --test-dir build --output-on-failure
# ou diretamente:
./build/test_converters
```

---

## 🖥️ Modos de Uso

### 1. Modo Interface Gráfica Ultra Leve e Moderna (GUI)
Interface escura, moderna e minimalista (Dear ImGui + GLFW/OpenGL) com suporte a Drag & Drop:
```bash
./build/OmniConverter
```
* **Arrastar e Soltar**: Arraste múltiplos extratos em lote diretamente para a janela.
* **Inspeção de Transações**: Clique em "Ver" em qualquer extrato para inspecionar, filtrar e auditar as transações antes ou depois da conversão.
* **Seletor de Formatos Alvo**: Marque quais formatos deseja gerar em paralelo (OFX, CSV, XLSX, QIF, PDF).

### 2. Modo Linha de Comando (CLI Headless / Alta Vazão)
Para automações, scripts de contabilidade e servidores:
```bash
# Converter lote de arquivos para OFX, CSV e XLSX
./build/OmniConverter --batch samples/extrato_itau.ofx samples/extrato_nubank.csv --to ofx,csv,xlsx --out ./dist

# Converter uma pasta inteira recursivamente
./build/OmniConverter --batch /caminho/com/extratos/ --to ofx,xlsx,qif,pdf --out /caminho/destino/
```

---

## 📁 Estrutura do Código

```
├── include/
│   ├── core/
│   │   ├── Types.hpp          # Representação Intermediária Canônica (IR), Metadados e Telemetria
│   │   ├── Hasher.hpp         # Hasher Criptográfico SHA-256 puro em C++
│   │   ├── Utils.hpp          # Analisador resiliente de valores (BRL/US), datas e strings
│   │   └── Pipeline.hpp       # ThreadPool concorrente com filas balanceadas
│   ├── parsers/               # Decodificadores Ingest (PDF, OFX, CSV, XLSX, QIF)
│   ├── encoders/              # Codificadores Egress (OFX, CSV, XLSX nativo, QIF, PDF vetorial)
│   ├── converter/
│   │   └── ConverterEngine.hpp# Motor unificado de coordenação, detecção e desduplicação
│   └── ui/
│       ├── Theme.hpp          # Paleta moderna Slate & Cyan Neumorphic
│       ├── UIController.hpp   # Controlador da visão, telemetria e grid de auditoria
│       └── AppWindow.hpp      # Janela GLFW com suporte a GLSL 1.20 / Legado
├── src/                       # Implementação dos módulos C++
├── tests/
│   └── test_converters.cpp    # Testes unitários de roundtrip e validação de hash
├── vendor/                    # ImGui e Miniz amalgamados sem dependências externas
└── CMakeLists.txt
```
