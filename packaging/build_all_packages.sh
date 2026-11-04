#!/bin/bash
# =================================================================================================
# BSC (Bank Statement Conversion) - Pipeline Automatizado de Empacotamento Multiplataforma
# Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao,
# Departamento de Eletronica e Sistemas, CTG-UFPE.
# =================================================================================================
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

mkdir -p releases

echo "[1/4] Gerando pacote Debian (.deb)..."
DEB_ROOT=/tmp/deb_pkg_root
rm -rf "$DEB_ROOT"
mkdir -p "$DEB_ROOT/DEBIAN" \
         "$DEB_ROOT/usr/bin" \
         "$DEB_ROOT/usr/share/applications" \
         "$DEB_ROOT/usr/share/icons/hicolor/scalable/apps" \
         "$DEB_ROOT/usr/share/pixmaps" \
         "$DEB_ROOT/usr/share/doc/bsc"

cp packaging/control "$DEB_ROOT/DEBIAN/control"
cp packaging/bsc.desktop "$DEB_ROOT/usr/share/applications/bsc.desktop"
cp packaging/bsc.svg "$DEB_ROOT/usr/share/icons/hicolor/scalable/apps/bsc.svg"
cp packaging/bsc.svg "$DEB_ROOT/usr/share/pixmaps/bsc.svg"
cp LICENSE "$DEB_ROOT/usr/share/doc/bsc/copyright"
strip -s -o "$DEB_ROOT/usr/bin/BSC" build/BSC
chmod 755 "$DEB_ROOT/usr/bin/BSC"

dpkg-deb --root-owner-group --build "$DEB_ROOT" releases/bsc_2.5.0_amd64.deb

echo "[2/4] Gerando pacote RPM (.rpm)..."
RPM_TOP=/tmp/rpm_build_dir
rm -rf "$RPM_TOP"
mkdir -p "$RPM_TOP"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS,tmp,db}

cp packaging/bsc.spec "$RPM_TOP/SPECS/bsc.spec"
sed -i "s|/home/matheus/Documentos/OFX/packaging/deb_root|$DEB_ROOT|g" "$RPM_TOP/SPECS/bsc.spec"

LD_LIBRARY_PATH=/tmp/rpmtools/usr/lib/x86_64-linux-gnu /tmp/rpmtools/usr/bin/rpmbuild \
    --rcfile /tmp/rpmtools/usr/lib/rpm/rpmrc \
    --macros /tmp/rpmtools/usr/lib/rpm/macros:/tmp/rpmtools/usr/lib/rpm/platform/x86_64-linux/macros \
    --define "_topdir $RPM_TOP" \
    --define "_build_id_links none" \
    --define "_tmppath $RPM_TOP/tmp" \
    --define "_dbpath $RPM_TOP/db" \
    --define "_fileattrsdir /tmp/rpmtools/usr/lib/rpm/fileattrs" \
    --define "__spec_install_post %{nil}" \
    --define "__os_install_post %{nil}" \
    --define "__check_files %{nil}" \
    -bb "$RPM_TOP/SPECS/bsc.spec"

cp "$RPM_TOP"/RPMS/x86_64/bsc-2.5.0-1.x86_64.rpm releases/

echo "[3/4] Gerando Tarball Portatil Universal Linux (.tar.gz)..."
TAR_ROOT=/tmp/tar_pkg_root
rm -rf "$TAR_ROOT"
mkdir -p "$TAR_ROOT/bsc-2.5.0-linux-x86_64"

cp "$DEB_ROOT/usr/bin/BSC" "$TAR_ROOT/bsc-2.5.0-linux-x86_64/BSC"
cp packaging/bsc.desktop "$TAR_ROOT/bsc-2.5.0-linux-x86_64/bsc.desktop"
cp packaging/bsc.svg "$TAR_ROOT/bsc-2.5.0-linux-x86_64/bsc.svg"
cp LICENSE "$TAR_ROOT/bsc-2.5.0-linux-x86_64/LICENSE"

cat << 'EOF' > "$TAR_ROOT/bsc-2.5.0-linux-x86_64/install.sh"
#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$HOME/.local/bin"
APP_DIR="$HOME/.local/share/applications"
ICON_DIR="$HOME/.local/share/icons/hicolor/scalable/apps"

mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR"
install -m 755 "$DIR/BSC" "$BIN_DIR/BSC"
install -m 644 "$DIR/bsc.svg" "$ICON_DIR/bsc.svg"

sed -e "s|^Exec=BSC|Exec=$BIN_DIR/BSC|" \
    -e "s|^Icon=bsc|Icon=$ICON_DIR/bsc.svg|" \
    "$DIR/bsc.desktop" > "$APP_DIR/bsc.desktop"
chmod +x "$APP_DIR/bsc.desktop"

echo "Instalacao concluida com sucesso em $BIN_DIR/BSC!"
EOF
chmod +x "$TAR_ROOT/bsc-2.5.0-linux-x86_64/install.sh"

cat << 'EOF' > "$TAR_ROOT/bsc-2.5.0-linux-x86_64/README.txt"
=================================================================================
BSC - Bank Statement Conversion (Versao 2.5.0)
Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao,
Departamento de Eletronica e Sistemas, CTG-UFPE.
=================================================================================

EXECUCAO DIRETA (SEM INSTALACAO):
    ./BSC

INSTALACAO OPCIONAL NO MENU DO SISTEMA:
    ./install.sh
=================================================================================
EOF

tar -czvf releases/bsc-2.5.0-linux-x86_64.tar.gz -C "$TAR_ROOT" bsc-2.5.0-linux-x86_64

echo "[4/4] Gerando pacote ZIP para Windows x86..."
WIN_ROOT=/tmp/win_pkg_root
rm -rf "$WIN_ROOT"
mkdir -p "$WIN_ROOT/bsc-2.5.0-windows-x86"

cp /tmp/bsc_win32_build/BSC.exe "$WIN_ROOT/bsc-2.5.0-windows-x86/BSC.exe"
cp LICENSE "$WIN_ROOT/bsc-2.5.0-windows-x86/LICENSE"

cat << 'EOF' > "$WIN_ROOT/bsc-2.5.0-windows-x86/README.txt"
=================================================================================
BSC - Bank Statement Conversion (Versao 2.5.0 para Windows x86 / x64)
Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao,
Departamento de Eletronica e Sistemas, CTG-UFPE.
=================================================================================

COMPATIBILIDADE:
- Windows 11, Windows 10, Windows 8.1, Windows 8, Windows 7 (32-bit e 64-bit).
- Binario nativo PE32 x86 100% estatico, sem necessidade de DLLs adicionais.

COMO USAR:
Basta dar dois cliques em "BSC.exe".
=================================================================================
EOF

(cd "$WIN_ROOT" && zip -r "$DIR/releases/bsc-2.5.0-windows-x86.zip" bsc-2.5.0-windows-x86/)

echo "[5/5] Gerando tabela criptografica SHA-256..."
cd releases
rm -f SHA256SUMS.txt
sha256sum bsc* > SHA256SUMS.txt
cat SHA256SUMS.txt
echo "Empacotamento concluido com sucesso!"
