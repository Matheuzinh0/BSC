# BSC - Bank Statement Conversion (v2.5)
## Sistema Universal de Conversao de Extratos Bancarios em C++20

**Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao, Departamento de Eletronica e Sistemas, Centro de Tecnologia e Geociencias, Universidade Federal de Pernambuco (CTG-UFPE).**

Licenca: **MIT License**

---

### 1. Descricao do Projeto

O **BSC (Bank Statement Conversion)** e um software de alto desempenho projetado para a ingestao, processamento e conversao em lote de extratos bancarios entre multiplos formatos financeiros. Construido integralmente em C++20 nativo, o sistema prioriza velocidade de execucao, eficiencia de memoria e seguranca estrita dos dados.

O software opera de maneira autonoma, sem runtimes pesados ou navegadores embutidos, assegurando execucao agil tanto em estacoes de trabalho modernas quanto em processadores legados com recursos restritos (como Intel Core 2 Duo, Pentium e Atom). O consumo tipico de memoria e inferior a 15 MB de RAM.

---

### 2. Matriz de Conversao Universal (Any-to-Any)

O BSC implementa uma arquitetura baseada em Modelo Intermediario Canonico (IR). Qualquer extrato de entrada e normalizado e pode ser convertido simultaneamente para qualquer combinacao dos formatos de destino suportados:

| Formato de Entrada / Saida | PDF | OFX | CSV | XLSX | QIF |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **PDF** | Sim | Sim | Sim | Sim | Sim |
| **OFX** | Sim | Sim | Sim | Sim | Sim |
| **CSV** | Sim | Sim | Sim | Sim | Sim |
| **XLSX**| Sim | Sim | Sim | Sim | Sim |
| **QIF** | Sim | Sim | Sim | Sim | Sim |

Formatos suportados:
- **PDF**: Analise inteligente de streams textuais com suporte a layouts bancarios nacionais (Banco do Brasil, Itau, Bradesco, Santander, Caixa, Nubank, Inter, Sicoob, Sicredi, BTG Pactual, Mercado Pago / Mercado Livre, etc.) e pipeline integrado de OCR (Optical Character Recognition) em C++ para processamento automatico de extratos digitalizados ou em formato raster/imagem. Geracao de relatorios vetoriais consolidados.
- **OFX (Open Financial Exchange)**: Conformidade com versoes 1.02 (SGML) e 2.x (XML), compativel com QuickBooks, ContaAzul, Omie e ERPs contabeis.
- **CSV (RFC 4180)**: Deteccao automatica de delimitador (ponto-e-virgula, virgula, tabulacao) e interpretacao de valores monetarios brasileiros e internacionais.
- **XLSX (Microsoft Excel OpenXML)**: Leitura e gravacao nativa em C++ sem dependencias externas pesadas, incluindo celulas estilizadas, cores para debitos/creditos e somatorios.
- **QIF (Quicken Interchange Format)**: Formato padrao para sistemas Quicken e GnuCash.

---

### 3. Principais Caracteristicas

- **Pipeline Hibrido de Reconhecimento Optico (OCR)**: Motor OCR nativo em C++ integrado com binarizacao adaptativa por imagem integral (Bradley-Roth), segmentacao de linhas/glifos por perfis de projecao, classificacao topologica com analise de lacos fechados e correcao contextual automatica para extratos digitalizados/escaneados ou convertidos em imagem. Aceleracao transparente via Tesseract quando disponivel no sistema.
- **Compatibilidade Completa com Fintechs e Mercado Livre**: Suporte nativo a extratos do Mercado Pago e Mercado Livre (codigo de compensacao 323), datas por extenso com preposicoes ("24 de outubro de 2023", "15 de out"), notacoes de valor com sinal invertido ("R$ -150,00"), identificadores de operacao (#...) e colunas de Entrada/Saida.
- **Processamento Concorrente em Lote**: ThreadPool auto-adaptativo que dimensiona o paralelismo de acordo com os nucleos da CPU, processando centenas de extratos por segundo.
- **Seguranca e Privacidade**: Operacao 100% local (air-gapped), sem conexoes de rede externas ou telemetria remota.
- **Integridade Criptografica**: Geracao de hash SHA-256 para cada transacao e extrato consolidado, garantindo rastreabilidade, auditoria e desduplicao.
- **Validacao Contabil**: Verificacao automatica de paridade entre saldo inicial, lancamentos e saldo final.
- **Execucao em Hardware Legado**: Interface grafica acelerada por hardware via GLFW e Dear ImGui compativel com OpenGL 2.1 e GLSL 1.20.

---

### 4. Download e Instalacao Rapida (Pacotes Prontos para Uso)

Os pacotes binarios pre-compilados estao disponiveis tanto na aba [Releases](https://github.com/Matheuzinh0/BSC/releases) do GitHub quanto diretamente na pasta [`releases/`](file:///home/matheus/Documentos/OFX/releases) do repositorio:

| Sistema Operacional | Formato do Pacote | Arquitetura | Link Direto para Download | Como Usar / Instalar |
| :--- | :--- | :--- | :--- | :--- |
| **Windows** (11, 10, 8, 7) | ZIP Portatil | x86 (32/64-bit) | [bsc-2.5.0-windows-x86.zip](https://github.com/Matheuzinh0/BSC/releases/download/v2.5.0/bsc-2.5.0-windows-x86.zip) | Descompactar e dar dois cliques em `BSC.exe` |
| **Ubuntu / Debian / Mint** | Pacote `.deb` | x86_64 | [bsc_2.5.0_amd64.deb](https://github.com/Matheuzinh0/BSC/releases/download/v2.5.0/bsc_2.5.0_amd64.deb) | `sudo dpkg -i bsc_2.5.0_amd64.deb` |
| **Fedora / RHEL / openSUSE**| Pacote `.rpm` | x86_64 | [bsc-2.5.0-1.x86_64.rpm](https://github.com/Matheuzinh0/BSC/releases/download/v2.5.0/bsc-2.5.0-1.x86_64.rpm) | `sudo rpm -ivh bsc-2.5.0-1.x86_64.rpm` |
| **Linux Universal (Todas)**| Tarball Portatil | x86_64 | [bsc-2.5.0-linux-x86_64.tar.gz](https://github.com/Matheuzinh0/BSC/releases/download/v2.5.0/bsc-2.5.0-linux-x86_64.tar.gz) | Descompactar e executar `./BSC` (ou `./install.sh`) |

#### Instrucoes Detalhadas para o Usuario:

##### Windows:
1. Baixe o arquivo `bsc-2.5.0-windows-x86.zip`.
2. Clique com o botao direito e selecione "Extrair Tudo...".
3. Abra a pasta extraida e de dois cliques em `BSC.exe`.
   - Nao e necessario instalar drivers ou programas adicionais. O aplicativo roda diretamente em computadores antigos ou modernos.

##### Ubuntu, Debian, Linux Mint e Pop!_OS:
1. Baixe o arquivo `bsc_2.5.0_amd64.deb`.
2. De dois cliques no arquivo para abrir com o instalador de programas da sua distribuicao, ou execute no terminal:
   ```bash
   sudo dpkg -i bsc_2.5.0_amd64.deb || sudo apt-get install -f -y
   ```
3. O atalho "BSC" aparecera automaticamente no menu de aplicativos do sistema.

##### Fedora, Red Hat, openSUSE e Rocky Linux:
1. Baixe o arquivo `bsc-2.5.0-1.x86_64.rpm`.
2. Execute no terminal:
   ```bash
   sudo dnf install ./bsc-2.5.0-1.x86_64.rpm
   # ou via rpm:
   sudo rpm -ivh bsc-2.5.0-1.x86_64.rpm
   ```

##### Qualquer Distribuicao Linux (Sem Necessidade de Permissao de Administrador):
1. Baixe o arquivo `bsc-2.5.0-linux-x86_64.tar.gz`.
2. Extraia com o gerenciador de arquivos ou via terminal:
   ```bash
   tar -xzvf bsc-2.5.0-linux-x86_64.tar.gz
   cd bsc-2.5.0-linux-x86_64
   ./BSC
   ```
3. (Opcional) Para criar o atalho no menu de aplicativos, execute `./install.sh`.

---

### 5. Compilacao

#### Requisitos
- Compilador C++ com suporte a C++20 (`g++` >= 10 ou `clang++` >= 11)
- `cmake` (versao >= 3.20)
- Bibliotecas do sistema: `libglfw3-dev`, `libgl1-mesa-dev`, `libx11-dev`

#### Instrucoes de Build
```bash
# Configuracao do CMake em modo Release
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compilacao paralela
cmake --build build -j$(nproc)
```

#### Testes Automatizados
```bash
ctest --test-dir build --output-on-failure
# ou execucao direta:
./build/test_converters
```

---

### 6. Modos de Uso

#### 6.1. Interface Grafica (GUI)
Interface moderna, escura e de baixa latencia com suporte a arrastar e soltar:
```bash
./build/BSC
```
- **Arrastar e Soltar**: Arraste arquivos ou pastas com extratos diretamente para a interface.
- **Inspecao de Transacoes**: Filtre, ordene e revise cada lancamento com identificador unico SHA-256.
- **Selecao Multipla**: Escolha quais formatos de saida deseja gerar simultaneamente e clique em "INICIAR CONVERSAO EM LOTE".

#### 6.2. Linha de Comando (Modo Batch Headless)
Ideal para scripts de conciliacao, servidores e automacoes:
```bash
# Converter arquivos especificos para OFX, CSV e XLSX
./build/BSC --batch extrato1.pdf extrato2.csv --to ofx,csv,xlsx --out ./saida

# Processar recursivamente um diretorio inteiro
./build/BSC --batch /caminho/extratos/ --to ofx,xlsx,qif,pdf --out ./saida
```

---

### 7. Estrutura do Codigo-Fonte

```
.
|-- CMakeLists.txt             # Configuracao de build do CMake
|-- LICENSE                    # Licenca MIT
|-- README.md                  # Documentacao do projeto
|-- include
|   |-- converter              # Orquestrador central e lote (ConverterEngine)
|   |-- core                   # Tipos canonicos, Hasher SHA-256, ThreadPool e utilitarios
|   |-- encoders               # Codificadores de saida (PDF, OFX, CSV, XLSX, QIF)
|   |-- ocr                    # Motor nativo de reconhecimento optico de caracteres em C++
|   |-- parsers                # Analisadores sintaticos de entrada (PDF, OFX, CSV, XLSX, QIF)
|   `-- ui                     # Interface grafica, tema e janelas (ImGui/GLFW)
|-- releases                   # Pacotes de distribuicao (.deb, .rpm, .tar.gz, .zip)
|-- scripts                    # Scripts de lote e geracao de extratos de teste
|-- src                        # Implementacao dos modulos em C++
|   |-- converter
|   |-- core
|   |-- encoders
|   |-- ocr
|   |-- parsers
|   `-- ui
|-- tests                      # Suite de testes de integridade e roundtrip
`-- vendor                     # Dependencias amalgamadas (ImGui e Miniz)
```

---

### 7. Licenca

Este projeto e distribuido sob a licenca **MIT**. Consulte o arquivo [LICENSE](file:///home/matheus/Documentos/OFX/LICENSE) para mais detalhes.

Desenvolvido por Matheus Sousa.
Laboratorio de Comunicacao e Seguranca da Informacao.
Departamento de Eletronica e Sistemas, Centro de Tecnologia e Geociencias (CTG).
Universidade Federal de Pernambuco (UFPE).
Recife - PE, Brasil.
