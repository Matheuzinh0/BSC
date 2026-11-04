#!/usr/bin/env python3
"""
=================================================================================================
Projeto: BSC (Bank Statement Conversion)
Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao,
Departamento de Eletronica e Sistemas, CTG-UFPE.
=================================================================================================
Gerador de Lotes Heterogeneos de Extratos Financeiros para Testes de Estresse e Conversao
Gera arquivos em PDF, OFX, CSV, XLSX e QIF para multiplos bancos brasileiros.
"""

import os
import sys
import random
import zipfile
from datetime import datetime, timedelta

BANKS = [
    {"name": "Banco do Brasil S.A.", "code": "001", "prefix": "BB"},
    {"name": "Itau Unibanco S.A.", "code": "341", "prefix": "ITAU"},
    {"name": "Banco Bradesco S.A.", "code": "237", "prefix": "BRADESCO"},
    {"name": "Caixa Economica Federal", "code": "104", "prefix": "CAIXA"},
    {"name": "Banco Santander Brasil", "code": "033", "prefix": "SANTANDER"},
    {"name": "Nu Pagamentos - Nubank", "code": "260", "prefix": "NUBANK"},
    {"name": "Banco Inter S.A.", "code": "077", "prefix": "INTER"},
    {"name": "Bancoob - Sicoob", "code": "756", "prefix": "SICOOB"},
    {"name": "Banco Sicredi", "code": "748", "prefix": "SICREDI"},
    {"name": "Banco BTG Pactual", "code": "208", "prefix": "BTG"},
    {"name": "Mercado Pago / Mercado Livre", "code": "323", "prefix": "MERCADOPAGO"},
]

TRANSACTIONS_POOL = [
    ("PIX RECEBIDO - CLIENTE ALFA", 450.00, "CREDIT"),
    ("PAGAMENTO DE FATURA ENERGIA ELETRICA", -189.45, "DEBIT"),
    ("TED RECEBIDA - SERVICOS DE CONSULTORIA", 2500.00, "CREDIT"),
    ("COMPRA CARTAO DEBITO POSTO COMBUSTIVEL", -210.00, "DEBIT"),
    ("SUPERMERCADO CENTRAL ALIMENTOS", -432.80, "DEBIT"),
    ("TRANSFERENCIA PIX FORNECEDOR BETA", -620.00, "DEBIT"),
    ("RENDIMENTO DE APLICACOES FINANCEIRAS", 78.90, "CREDIT"),
    ("PAGAMENTO TELEFONIA E FIBRA OPTICA", -119.90, "DEBIT"),
    ("PIX RECEBIDO TRANSFERENCIA PESSOAL", 150.00, "CREDIT"),
    ("COMPRA FARMACIA POPULAR SAUDE", -67.30, "DEBIT"),
    ("REEMBOLSO DESPESAS CORPORATIVAS", 345.50, "CREDIT"),
    ("TARIFA MENSALIDADE PACOTE DE SERVICOS", -29.90, "DEBIT"),
    ("VENDA MERCADO LIVRE PRODUTO #718293", 320.00, "CREDIT"),
    ("TARIFA DE ENVIO MERCADO ENVIOS", -34.90, "DEBIT"),
    ("RENDIMENTO DE CONTA MERCADO PAGO", 12.80, "CREDIT"),
]

def generate_csv(file_path, bank, tx_count=10):
    lines = ["Data;Documento;Descricao;Valor;Saldo"]
    base_date = datetime(2023, 10, 1)
    running_balance = 5000.00
    for i in range(tx_count):
        dt = base_date + timedelta(days=i * 2)
        desc, amt, _ = random.choice(TRANSACTIONS_POOL)
        running_balance += amt
        amt_str = f"{amt:,.2f}".replace(",", "X").replace(".", ",").replace("X", ".")
        bal_str = f"{running_balance:,.2f}".replace(",", "X").replace(".", ",").replace("X", ".")
        doc = f"{random.randint(100000, 999999)}"
        lines.append(f"{dt.strftime('%d/%m/%Y')};{doc};{desc};{amt_str};{bal_str}")
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

def generate_ofx(file_path, bank, tx_count=10):
    dt_now = datetime.now().strftime("%Y%m%d%H%M%S")
    base_date = datetime(2023, 10, 1)
    tx_lines = []
    total = 0.0
    for i in range(tx_count):
        dt = base_date + timedelta(days=i * 2)
        desc, amt, tx_type = random.choice(TRANSACTIONS_POOL)
        total += amt
        dt_str = dt.strftime("%Y%m%d120000[-03:EST]")
        fitid = f"FIT{dt.strftime('%Y%m%d')}{i:04d}"
        tx_lines.append(f"""<STMTTRN>
<TRNTYPE>{tx_type}
<DTPOSTED>{dt_str}
<TRNAMT>{amt:.2f}
<FITID>{fitid}
<MEMO>{desc}
</STMTTRN>""")

    ofx_content = f"""OFXHEADER:100
DATA:OFXSGML
VERSION:102
SECURITY:NONE
ENCODING:USASCII
CHARSET:1252
COMPRESSION:NONE
OLDFILEUID:NONE
NEWFILEUID:NONE

<OFX>
<SIGNONMSGSRSV1>
<SONRS>
<STATUS>
<CODE>0
<SEVERITY>INFO
</STATUS>
<DTSERVER>{dt_now}
<LANGUAGE>POR
</SONRS>
</SIGNONMSGSRSV1>
<BANKMSGSRSV1>
<STMTTRNRS>
<TRNUID>1001
<STATUS>
<CODE>0
<SEVERITY>INFO
</STATUS>
<STMTRS>
<CURDEF>BRL
<BANKACCTFROM>
<BANKID>{bank['code']}
<BRANCHID>1234
<ACCTID>98765-4
<ACCTTYPE>CHECKING
</BANKACCTFROM>
<BANKTRANLIST>
<DTSTART>20231001120000[-03:EST]
<DTEND>20231031120000[-03:EST]
{''.join(tx_lines)}
</BANKTRANLIST>
<LEDGERBAL>
<BALAMT>{total:.2f}
<DTASOF>{dt_now}
</LEDGERBAL>
</STMTRS>
</STMTTRNRS>
</BANKMSGSRSV1>
</OFX>"""
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(ofx_content)

def generate_qif(file_path, bank, tx_count=10):
    lines = ["!Type:Bank"]
    base_date = datetime(2023, 10, 1)
    for i in range(tx_count):
        dt = base_date + timedelta(days=i * 2)
        desc, amt, _ = random.choice(TRANSACTIONS_POOL)
        doc = f"{random.randint(100000, 999999)}"
        lines.append(f"D{dt.strftime('%d/%m/%Y')}")
        lines.append(f"T{amt:.2f}")
        lines.append(f"N{doc}")
        lines.append(f"P{desc}")
        lines.append("MTransferencia Eletronica")
        lines.append("^")
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

def generate_pdf(file_path, bank, tx_count=10):
    # Generates a standard valid PDF containing layout-agnostic financial statement records
    base_date = datetime(2023, 10, 1)
    text_lines = [
        f"{bank['name'].upper()} - AUTOATENDIMENTO",
        f"Agencia: 1234-5    Conta Corrente: 98765-4",
        "Data        Documento  Historico                       Valor",
        "SALDO ANTERIOR                                         5.000,00 C"
    ]
    for i in range(tx_count):
        dt = base_date + timedelta(days=i * 2)
        desc, amt, _ = random.choice(TRANSACTIONS_POOL)
        doc = f"{random.randint(100000, 999999)}"
        indicator = "C" if amt >= 0 else "D"
        amt_abs = abs(amt)
        amt_str = f"{amt_abs:,.2f}".replace(",", "X").replace(".", ",").replace("X", ".") + f" {indicator}"
        text_lines.append(f"{dt.strftime('%d/%m/%Y')}  {doc}     {desc:<30}  {amt_str:>12}")
        if random.random() < 0.3:
            text_lines.append(f"FAVORECIDO: EMPRESA PARCEIRA {i+1} LTDA")
    text_lines.append("SALDO ATUAL                                            7.850,00 C")

    stream_content = "BT\n/F1 10 Tf\n50 750 Td\n15 TL\n"
    for line in text_lines:
        safe_line = line.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")
        stream_content += f"({safe_line}) '\n"
    stream_content += "ET\n"

    stream_bytes = stream_content.encode("latin1")
    stream_len = len(stream_bytes)

    pdf = bytearray()
    pdf.extend(b"%PDF-1.4\n")
    offsets = []

    def add_obj(content):
        offsets.append(len(pdf))
        pdf.extend(content)

    add_obj(b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n")
    add_obj(b"2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n")
    add_obj(b"3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n")
    add_obj(f"4 0 obj\n<< /Length {stream_len} >>\nstream\n".encode("latin1") + stream_bytes + b"\nendstream\nendobj\n")
    add_obj(b"5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n")

    xref_offset = len(pdf)
    pdf.extend(f"xref\n0 {len(offsets) + 1}\n0000000000 65535 f \n".encode("latin1"))
    for off in offsets:
        pdf.extend(f"{off:010d} 00000 n \n".encode("latin1"))
    pdf.extend(f"trailer\n<< /Size {len(offsets) + 1} /Root 1 0 R >>\nstartxref\n{xref_offset}\n%%EOF\n".encode("latin1"))

    with open(file_path, "wb") as f:
        f.write(pdf)

def generate_xlsx(file_path, bank, tx_count=10):
    base_date = datetime(2023, 10, 1)
    shared_strings = [
        "Data", "Documento", "Descricao", "Valor", "Saldo"
    ]
    rows_xml = []
    # Header row
    rows_xml.append('<row r="1">')
    for c_idx in range(5):
        col_letter = chr(ord('A') + c_idx)
        rows_xml.append(f'<c r="{col_letter}1" t="s"><v>{c_idx}</v></c>')
    rows_xml.append('</row>')

    str_map = {s: idx for idx, s in enumerate(shared_strings)}

    for r_idx in range(2, 2 + tx_count):
        dt = base_date + timedelta(days=(r_idx - 2) * 2)
        desc, amt, _ = random.choice(TRANSACTIONS_POOL)
        doc = f"{random.randint(100000, 999999)}"
        dt_str = dt.strftime("%Y-%m-%d")

        for val in [dt_str, doc, desc]:
            if val not in str_map:
                str_map[val] = len(shared_strings)
                shared_strings.append(val)

        rows_xml.append(f'<row r="{r_idx}">')
        rows_xml.append(f'<c r="A{r_idx}" t="s"><v>{str_map[dt_str]}</v></c>')
        rows_xml.append(f'<c r="B{r_idx}" t="s"><v>{str_map[doc]}</v></c>')
        rows_xml.append(f'<c r="C{r_idx}" t="s"><v>{str_map[desc]}</v></c>')
        rows_xml.append(f'<c r="D{r_idx}"><v>{amt:.2f}</v></c>')
        rows_xml.append(f'<c r="E{r_idx}"><v>{(5000.0 + amt):.2f}</v></c>')
        rows_xml.append('</row>')

    sst_entries = "".join(f"<si><t>{s}</t></si>" for s in shared_strings)
    sst_xml = f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count="{len(shared_strings)}" uniqueCount="{len(shared_strings)}">{sst_entries}</sst>'

    sheet_xml = f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>{"".join(rows_xml)}</sheetData></worksheet>'

    content_types = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/></Types>'

    workbook_xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheets><sheet name="Extrato" sheetId="1" r:id="rId1" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"/></sheets></workbook>'

    rels_xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>'

    wb_rels = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/></Relationships>'

    with zipfile.ZipFile(file_path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", content_types)
        z.writestr("_rels/.rels", rels_xml)
        z.writestr("xl/workbook.xml", workbook_xml)
        z.writestr("xl/_rels/workbook.xml.rels", wb_rels)
        z.writestr("xl/worksheets/sheet1.xml", sheet_xml)
        z.writestr("xl/sharedStrings.xml", sst_xml)

def main():
    target_dir = sys.argv[1] if len(sys.argv) > 1 else "test_batch"
    count_per_format = int(sys.argv[2]) if len(sys.argv) > 2 else 10

    os.makedirs(target_dir, exist_ok=True)
    print(f"Gerando lote de teste em: {target_dir}")
    print(f"Total por formato: {count_per_format} arquivos (PDF, OFX, CSV, XLSX, QIF)")

    total_created = 0
    generators = [
        ("pdf", generate_pdf),
        ("ofx", generate_ofx),
        ("csv", generate_csv),
        ("xlsx", generate_xlsx),
        ("qif", generate_qif),
    ]

    for fmt_name, gen_fn in generators:
        for idx in range(1, count_per_format + 1):
            bank = BANKS[(idx - 1) % len(BANKS)]
            fname = f"extrato_{bank['prefix'].lower()}_{idx:02d}.{fmt_name}"
            fpath = os.path.join(target_dir, fname)
            tx_count = random.randint(8, 25)
            gen_fn(fpath, bank, tx_count)
            total_created += 1

    print(f"Lote gerado com sucesso! Total de {total_created} arquivos criados em {target_dir}/")

if __name__ == "__main__":
    main()
