# Aggregator for the RPM20-GS variant's test binaries.
# Each sub-pro builds independently; the main rf8000.pro is unaffected.

TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS = \
    unitconverter \
    calibrationmodel \
    advanced_calibration_adapter \
    cablemodel \
    pmdevicefactory \
    rf8000_parser \
    rfpmv5_parser \
    rfpmv7_parser \
    properties_validation \
    conceptrfrpm_parser \
    attenuator_chain \
    attdevice_parser
