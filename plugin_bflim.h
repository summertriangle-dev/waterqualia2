int   bflim_test_eligibility(const uint8_t *stream, size_t len);
char *bflim_out_filename(const char *archive, const char *intname, const char *outputdir);
int   bflim_extract(const char *output_file, const uint8_t *stream, size_t len);