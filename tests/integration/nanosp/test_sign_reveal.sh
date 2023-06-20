# full input: 0300000000000000000000000000000000000000000000000000000000000000006b00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400747884d9abdf16b3ab745158925f567e222f71225501826fa83347f6cbe9c393
# signer: tz1dyX3B1CFYa2DfdFLyPtiJCfQRUgPVME6E
expect_full_text 'Tezos Wallet' 'ready for' 'safe signing'
send_async_apdus \
	800f000011048000002c800006c18000000080000000 "expect_apdu_return 9000
"\
	800f81005d0300000000000000000000000000000000000000000000000000000000000000006b00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400747884d9abdf16b3ab745158925f567e222f71225501826fa83347f6cbe9c393 "expect_apdu_return 28a91fe25dca9feed9a746d2825f113b0b7c0c534853d4d9e8d37f3a29119a3edc717466cf0fb90cd9ea8fcaf5252a52ae865901d28f54128498a94588530a48eb56c343497ff3f69a671f97b4a5e4dec0f7afb443f6658f62610287c829b6089000
"
expect_section_content nanosp 'Operation (0)' 'Reveal'
press_button right
expect_section_content nanosp 'Fee' '0.01 tz'
press_button right
expect_section_content nanosp 'Storage limit' '4'
press_button right
expect_section_content nanosp 'Public key' 'edpkuXX2VdkdXzkN11oLCb8Aurdo1BTAtQiK8ZY9UPj2YMt3AHEpcY'
press_button right
expect_full_text 'Accept?' 'Press both buttons' 'to accept.'
press_button both
expect_async_apdus_sent