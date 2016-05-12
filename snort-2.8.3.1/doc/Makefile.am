## $Id$
AUTOMAKE_OPTIONS=foreign no-dependencies

EXTRA_DIST = \
faq.pdf \
faq.tex \
snort_manual.pdf \
snort_manual.tex \
snort_schema_v106.pdf

dist_doc_DATA = \
AUTHORS \
BUGS \
CREDITS \
generators \
INSTALL \
NEWS \
PROBLEMS \
README \
README.alert_order \
README.ARUBA \
README.asn1 \
README.csv \
README.database \
README.dcerpc \
README.decode \
README.decoder_preproc_rules \
README.dns \
README.event_queue \
README.FLEXRESP \
README.FLEXRESP2 \
README.flow \
README.flowbits \
README.flow-portscan \
README.frag3 \
README.ftptelnet \
README.gre \
README.http_inspect \
README.INLINE \
README.ipip \
README.ipv6 \
README.pcap_readmode \
README.PerfProfiling \
README.PLUGINS \
README.ppm \
README.sfportscan \
README.SMTP \
README.ssh \
README.ssl \
README.stream4 \
README.stream5 \
README.tag \
README.thresholding \
README.UNSOCK \
README.variables \
README.WIN32 \
README.wireless \
TODO \
USAGE \
WISHLIST

DISTCLEANFILES= snort_manual.log snort_manual.toc snort_manual.aux faq.log faq.toc faq.aux snort_manual.pdf faq.pdf snort_manual.out

docdir = ${datadir}/doc/${PACKAGE}

SUFFIXES = .tex .dvi .ps

.tex.dvi:
	pslatex $< && pslatex $< && pslatex $<

.dvi.ps:
	dvips $< -o

.ps.pdf:
	ps2pdf $<

# perhaps one day, we will have a Makefile in the signatures directory...
# SUBDIRS = signatures
