# full input: 050200000135072f02000000c502000000c0034c072c0200000040051f02000000390200000034052000010743036c030b07430359030a07430368010000001b3b4c5c533f70242d4671295644670a5d74650a6f3476305f3829220200000072020000006d052000020743036c030b07430359030307430368010000002b4c692d252a656446367e3f455b354b6d753f64797669774a5e32225c64244679513e3e213e4424672851670743036801000000202a54783c45605369473659662a415e6b5a5c3d373f485b6d4f6c510a5d4568730200000062072f020000000203210200000054020000004f052000040743036c030b07430359030a074303680100000009225c365f340a246b25074303680100000024635e31225c3f45795f31214556627e393b45583b59550a234b6a325a54386860552158200345
# full output: {IF_NONE {{SWAP;IF {DIP {{DROP 1;PUSH unit Unit;PUSH bool True;PUSH string ";L\\S?p$-Fq)VDg\n]te\no4v0_8)\""}}} {{DROP 2;PUSH unit Unit;PUSH bool False;PUSH string "Li-%*edF6~?E[5Kmu?dyviwJ^2\"\\d$FyQ>>!>D$g(Qg";PUSH string "*Tx<E`SiG6Yf*A^kZ\\=7?H[mOlQ\n]Ehs"}}}} {IF_NONE {DUP} {{DROP 4;PUSH unit Unit;PUSH bool True;PUSH string "\"\\6_4\n$k%";PUSH string "c^1\"\\?Ey_1!EVb~9;EX;YU\n#Kj2ZT8h`U!X "}}};SIZE}
# signer: tz1dyX3B1CFYa2DfdFLyPtiJCfQRUgPVME6E
send_async_apdus \
        800f000011048000002c800006c18000000080000000 "expect_apdu_return 9000
"\
        800f0100eb050200000135072f02000000c502000000c0034c072c0200000040051f02000000390200000034052000010743036c030b07430359030a07430368010000001b3b4c5c533f70242d4671295644670a5d74650a6f3476305f3829220200000072020000006d052000020743036c030b07430359030307430368010000002b4c692d252a656446367e3f455b354b6d753f64797669774a5e32225c64244679513e3e213e4424672851670743036801000000202a54783c45605369473659662a415e6b5a5c3d373f485b6d4f6c510a5d4568730200000062072f020000000203210200000054020000004f05 "expect_apdu_return 9000
"\
        800f8200502000040743036c030b07430359030a074303680100000009225c365f340a246b25074303680100000024635e31225c3f45795f31214556627e393b45583b59550a234b6a325a54386860552158200345 "expect_apdu_return dcba96786c40c7894527a85050bc0b75eaee37aa020a52c634275450018da47822d552099146915f4c3280b3dbd4ab5d459b96b585247e58fda9bf53c9951dbe017811d074e688f11d8f47ce7f94c0548dca76be9ae9e87df794d3facdb4bb0a9000
"

expect_full_text 'Data' '{lF_NONE {{WAP;lF {DlP {{DROP 1;PUH unit Unit;PUH bool True;PUH string "'
press_button right
expect_full_text 'Data' ';L\\?p$-Fq)VDg\n]te\no4v0_8)\""}}} {{DROP 2;PUH unit Unit;PUH bool False;'
press_button right
expect_full_text 'Data' 'PUH string "Li-%*edF6~?E[5Kmu?dyviwJ^2\"\\d$FyQ>>!>D$g(Qg";PUH string "*Tx'
press_button right
expect_full_text 'Data' '<E`iG6Yf*A^kZ\\=7?H[mOlQ\n]Ehs"}}}} {lF_NONE {DUP} {{DROP 4;PUH unit Unit;'
press_button right
expect_full_text 'Data' 'PUH bool True;PUH string "\"\\6_4\n$k%";PUH string "c^1\"\\?Ey_1!EVb~9;EX'
press_button right
expect_full_text 'Data' ';YU\n#Kj2ZT8h`U!X "}}};lZE}'
press_button right

expect_full_text 'Accept?' 'Press both buttons' 'to accept.'
press_button both
expect_async_apdus_sent