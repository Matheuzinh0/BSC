# OmniStatement Studio (v2.5)
## Sistema de Conversao Universal de Extratos Financeiros e Alta Vazao em C++20

**Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao, Departamento de Eletronica e Sistemas, Centro de Tecnologia e Geociencias, Universidade Federal de Pernambuco (CTG-UFPE).**

---

### 1. Visao Geral e Fundamentacao Teorica

O OmniStatement Studio consiste em um sistema de processamento de fluxos de dados financeiros concebido sob os principios classicos da Engenharia de Telecomunicacoes e da Teoria da Informacao. O projeto visa mitigar perdas de dados e sobrecarga computacional na transformacao em lote de extratos de formatos heterogeneos, provendo integridade de sinal ponta a ponta, desduplicacao deterministica e alta taxa de transferencia com baixo consumo de ciclos de instrucao.

Projetado em C++20 estrito, o motor opera de maneira nativa, autonoma e desacoplada de interpretadores ou runtimes virtuais. O software assegura execucao eficiente em arquiteturas de computacao contemporaneas e legadas (processadores x86_64 restritos, incluindo linhas Intel Core 2 Duo, Pentium e Celeron), mantendo pegada de memoria inferior a 15 MB e tempo de inicializacao sub-milisegundo.

---

### 2. Principios de Engenharia de Telecomunicacoes Aplicados

#### 2.1. Arquitetura Pipelined e Capacidade de Canal
O fluxo de tratamento e estruturado em estagios independentes desacoplados, minimizando overhead temporal:
```
+---------------+     +--------------------+     +-------------------+
|  Ingestao e   | --> |  Demultiplexacao   | --> |  Representacao    |
|  Enquadramento|     |  e Tokenizacao     |     |  Canonica (IR)    |
+---------------+     +--------------------+     +-------------------+
                                                           |
+---------------+     +--------------------+               |
| Saida e       | <-- |  Multiplexacao e   | <-------------+
| Serializacao  |     |  Roteamento Egress |
+---------------+     +--------------------+
```
- **Zero-Copy e Janelamento**: Extracao direta de tokens por meio de janelas `std::string_view`, prevenindo re-alocacoes dinamicas no heap e garantindo localidade espacial e temporal em caches L1/L2.
- **Descompressao In-Memory**: Reconstrucao de streams de texto comprimidos sob o algoritmo DEFLATE (RFC 1951) de documentos PDF e arquivos OpenXML diretamente em memoria antes da etapa sintatica.

#### 2.2. Concorrencia Adaptativa
O gerenciador de execucao `core::ThreadPool` computa a taxa de paralelismo em tempo de execucao atraves de `std::thread::hardware_concurrency()`. Em processadores legados de baixa contagem de nucleos (1 a 2 cores), o escalonamento ajusta o enfileiramento para mitigar trocas de contexto (*context switching*), garantindo throughput continuo superior a 2.000 transacoes por segundo.

#### 2.3. Seguranca da Informacao, Criptografia e Paridade
- **Ambiente Estritamente Air-Gapped**: Execucao 100% local e deterministica, desprovida de sockets de rede ou comunicacao remota.
- **Assinatura Digital SHA-256**: Cada registro transacional recebe um fingerprint criptografico calculado atraves da funcao hash segura SHA-256 (FIPS 180-4):
  $$\text{Hash}_{\text{tx}} = \text{SHA-256}(\text{Data} \parallel \text{Valor} \parallel \text{Documento} \parallel \text{Historico})$$
- **Verificacao de Paridade de Balanco (Ledger Balance Parity)**: O sistema monitora a consistencia contabil da serie temporal, aferindo a igualdade de estado:
  $$\text{Saldo}_{\text{final}} = \text{Saldo}_{\text{inicial}} + \sum_{k=1}^{N} \text{Transacao}_{k}$$
  Discrepancias sinalizam falha de integridade documental antes da serializacao.

---

### 3. Matriz Universal de Conversao (Any-to-Any)

O nucleo do sistema implementa o padrao de isolamento canonico, no qual qualquer analisador sintatico de entrada gera uma estrutura `core::Statement` normalizada, apta a ser serializada para qualquer um dos codificadores disponiveis:

| Entrada / Saida | PDF | OFX | CSV | XLSX | QIF |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **PDF** | Sim | Sim | Sim | Sim | Sim |
| **OFX** | Sim | Sim | Sim | Sim | Sim |
| **CSV** | Sim | Sim | Sim | Sim | Sim |
| **XLSX**| Sim | Sim | Sim | Sim | Sim |
| **QIF** | Sim | Sim | Sim | Sim | Sim |

Formatos e protocolos suportados:
- **PDF (ISO 32000-1)**: Descompressao de fluxos FlateDecode e decodificacao de operadores textuais com heuristica tabular para extratos bancarios nacionais e internacionais.
- **OFX (Open Financial Exchange)**: Conformidade com especificacoes OFX 1.02 (SGML) e OFX 2.x (XML).
- **CSV (RFC 4180)**: Analisador com inferencia automatica de delimitador (ponto-e-virgula, virgula, tabulacao) e decodificacao de moeda em padrao brasileiro ou internacional.
- **XLSX (ISO/IEC 29500 / ECMA-376)**: Leitura e gravacao nativa de pacotes OpenXML/ZIP contendo tabelas estruturadas, formatacao monetaria de celulas e somatorios.
- **QIF (Quicken Interchange Format)**: Protocolo padrao de transacoes financeiras `!Type:Bank`.

---

### 4. Compilacao e Construcao do Sistema

#### 4.1. Requisitos de Ferramental
- Compilador C++ com suporte integral a C++20 (`g++` >= 10 ou `clang++` >= 11)
- Kit de construcao `cmake` (versao >= 3.20)
- Bibliotecas graficas de sistema: `libglfw3-dev`, `libgl1-mesa-dev`, `libx11-dev`

#### 4.2. Procedimento de Construcao
```bash
# Configuracao do projeto em modo Release com otimizacoes de velocidade (-O3)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compilacao paralela utilizando todos os nucleos de processamento
cmake --build build -j$(nproc)
```

#### 4.3. Execucao dos Testes de Integridade
A suite de testes valida os vetores criptograficos, a normalizacao de dados e o roundtrip completo entre todos os formatos:
```bash
ctest --test-dir build --output-on-failure
# ou execucao direta do binario:
./build/test_converters
```

---

### 5. Modos de Operacao

#### 5.1. Interface Grafica (GUI)
Interface de alta reatividade desenvolvida sobre GLFW e Dear ImGui, configurada com perfil de compatibilidade para **OpenGL 2.1 / GLSL 1.20**:
```bash
./build/OmniConverter
```
Recursos da interface:
- **Arraste e Solte (Drag-and-Drop)**: Enfileiramento automatico de arquivos ou pastas arrastados diretamente sobre a janela.
- **Painel de Telemetria**: Medicao de taxa de transferencia em tempo real (Tx/s, MB/s e tempo total em milissegundos).
- **Inspecao Estruturada de Lote**: Visualizador com busca e filtro por campos de data, historico e valor, com indicacao visual de debito e credito e exibicao do checksum SHA-256.

#### 5.2. Modo em Linha de Comando (Headless Batch)
Projetado para ambientes de servidor, scripts de esteira contabil e processamento em lote silencioso:
```bash
# Conversao simultanea de multiplos arquivos para OFX, CSV e XLSX
./build/OmniConverter --batch samples/extrato_itau.ofx samples/extrato_nubank.csv --to ofx,csv,xlsx --out ./dist

# Processamento recursivo de todo um diretorio para todos os formatos suportados
./build/OmniConverter --batch /caminho/com/extratos/ --to ofx,csv,xlsx,qif,pdf --out /caminho/destino/
```

---

### 6. Estrutura de Diretorios

```
.
|-- CMakeLists.txt
|-- README.md
|-- include
|   |-- converter
|   |   `-- ConverterEngine.hpp    # Orquestrador unificado e gerenciamento de tarefas
|   |-- core
|   |   |-- Hasher.hpp             # Modulo criptografico SHA-256 nativo
|   |   |-- Pipeline.hpp           # ThreadPool com enfileiramento concorrente
|   |   |-- Types.hpp              # Modelo Intermediario Canonico (IR) e Telemetria
|   |   `-- Utils.hpp              # Analisador de valores, datas ISO 8601 e I/O
|   |-- encoders                   # Serializadores de saida (OFX, CSV, XLSX, QIF, PDF)
|   |-- parsers                    # Decodificadores de entrada (PDF, OFX, CSV, XLSX, QIF)
|   `-- ui                         # Controladores de janela GLFW e renderizacao ImGui
|-- samples                        # Amostras reais de teste para validacao de formatos
|-- src                            # Implementacao dos modulos de engenharia
|-- tests                          # Suite de testes automatizados e afericao de taxa
`-- vendor                         # Modulos de interface e compressao amalgamados
```

---

### 7. Licenca e Creditos Academicos

Desenvolvido por Matheus Sousa.
Laboratorio de Comunicacao e Seguranca da Informacao.
Departamento de Eletronica e Sistemas, Centro de Tecnologia e Geociencias (CTG).
Universidade Federal de Pernambuco (UFPE).
Recife - PE, Brasil.
