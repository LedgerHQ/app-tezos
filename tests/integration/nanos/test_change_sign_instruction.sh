start_speculos "$seed"
expect_full_text 'ready for' 'safe signing'
# INS_SIGN_WITH_HASH
send_apdu 800f000011048000002c800006c18000000080000000
expect_apdu_return 9000
# INS_SIGN
send_apdu 800481005e0300000000000000000000000000000000000000000000000000000000000000006c016e8874874d31c3fbd636e924d5a036a43ec8faa7d0860308362d80d30e01000000000000000000000000000000000000000000ff02000000020316
expect_apdu_return 6d00
# INS_SIGN
send_apdu 8004000011048000002c800006c18000000080000000
expect_apdu_return 9000
# INS_SIGN_WITH_HASH
send_apdu 800f81005e0300000000000000000000000000000000000000000000000000000000000000006c016e8874874d31c3fbd636e924d5a036a43ec8faa7d0860308362d80d30e01000000000000000000000000000000000000000000ff02000000020316
expect_apdu_return 6d00
press_button right
press_button right
press_button both
expect_exited
