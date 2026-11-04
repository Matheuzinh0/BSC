Name:           bsc
Version:        2.5.0
Release:        1
Summary:        Bank Statement Conversion (BSC)
License:        MIT
URL:            https://github.com/Matheuzinh0/BSC

Requires:       glibc >= 2.31, mesa-libGL

%description
Ultra-lightweight and high-throughput financial statement conversion
engine (PDF, OFX, CSV, XLSX, QIF) with graphical user interface and
headless batch processing CLI.
Developed by Matheus Sousa, Laboratorio de Comunicacao e Seguranca
da Informacao, Departamento de Eletronica e Sistemas, CTG-UFPE.

%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/share/applications
mkdir -p %{buildroot}/usr/share/icons/hicolor/scalable/apps
mkdir -p %{buildroot}/usr/share/pixmaps
mkdir -p %{buildroot}/usr/share/doc/bsc

install -m 755 /home/matheus/Documentos/OFX/packaging/deb_root/usr/bin/BSC %{buildroot}/usr/bin/BSC
install -m 644 /home/matheus/Documentos/OFX/packaging/deb_root/usr/share/applications/bsc.desktop %{buildroot}/usr/share/applications/bsc.desktop
install -m 644 /home/matheus/Documentos/OFX/packaging/deb_root/usr/share/icons/hicolor/scalable/apps/bsc.svg %{buildroot}/usr/share/icons/hicolor/scalable/apps/bsc.svg
install -m 644 /home/matheus/Documentos/OFX/packaging/deb_root/usr/share/pixmaps/bsc.svg %{buildroot}/usr/share/pixmaps/bsc.svg
install -m 644 /home/matheus/Documentos/OFX/LICENSE %{buildroot}/usr/share/doc/bsc/LICENSE

%files
%defattr(-,root,root,-)
/usr/bin/BSC
/usr/share/applications/bsc.desktop
/usr/share/icons/hicolor/scalable/apps/bsc.svg
/usr/share/pixmaps/bsc.svg
/usr/share/doc/bsc/LICENSE

%changelog
* Wed Nov 04 2026 Matheus Sousa <matheus@ufpe.br> - 2.5.0-1
- Initial RPM release of BSC
