idf_component_register(
    SRCS "uploader.c" "upload_index.c" "upload_scan.c" "upload_ox.c" "upload_sched.c"
         "upload_migrate.c" "upload_http.c" "upload_stage.c" "br_parse.c"
         "uploader_smb.c" "uploader_sleephq.c" "uploader_aerivue.c"
         "uploader_breathingroom.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash json esp-tls mbedtls libsmb2 esp_timer
)
