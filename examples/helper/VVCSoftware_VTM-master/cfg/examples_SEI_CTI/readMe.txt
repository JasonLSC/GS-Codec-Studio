example encoding command line
encoder -c encoder_randomaccess_vtm.cfg -c classH1.cfg -c H1_BalloonFestival.cfg -c seiCti_hdrPq_to_sdr1.cfg
        -i BalloonFestival_1920x1080p_24_10b_pq_709_ct2020_420_rev1.yuv -ip 32 -fs 0 -f 33 -q 22 
        -b BalloonFestival_1920x1080p_24_10b_pq_709_ct2020_420_rev1.bin -o /dev/null --InternalBitDepth=10 --OutputBitDepth=10 

example decoding command line
decoder -b BalloonFestival_1920x1080p_24_10b_pq_709_ct2020_420_rev1.bin -o dec.yuv --SEICTIFilename=dec_cti.yuv