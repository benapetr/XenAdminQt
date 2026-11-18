#ifndef TEST_OVFDATA_H
#define TEST_OVFDATA_H

// OVF XML fixtures for the ovfXenConfig_* tests.
//
// They live in a separate translation unit because moc cannot parse raw string
// literals that contain '"' followed by "//" (e.g. xmlns="http://..."): it treats
// the inner quote as the end of the string and the rest as a comment, finds no
// Q_OBJECT class and silently generates an empty test_main.moc. moc tokenizes
// even #ifndef Q_MOC_RUN blocks, so the strings must not be in test_main.cpp.

extern const char* const kOvfXml_ovfXenConfig_qtFormat_parsesHvmBootPolicy;
extern const char* const kOvfXml_ovfXenConfig_qtFormat_pvBootloader;
extern const char* const kOvfXml_ovfXenConfig_csFormat_parsesBootPolicy;
extern const char* const kOvfXml_ovfXenConfig_csFormat_recommendationsKeyExcluded;
extern const char* const kOvfXml_ovfXenConfig_noXenSection_returnsEmptyMap;
extern const char* const kOvfXml_ovfXenConfig_multipleVms_configsAreSeparate;
extern const char* const kOvfXml_ovfXenConfig_emptyValueAllowed;

#endif // TEST_OVFDATA_H
