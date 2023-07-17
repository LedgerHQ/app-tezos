# full input: 00000000000000000000000000000000000000000000000000000000000000009e00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400000002037a0000000a076501000000013100020000ffdd6102321bc251e4a5190ad5b12b251069d9b401010000000000000000000000000000000000000000000000000764656661756c74
# signer: tz1dyX3B1CFYa2DfdFLyPtiJCfQRUgPVME6E
start_speculos "$seed"
expect_full_text 'Tezos Wallet' 'ready for' 'safe signing'
send_async_apdus \
	800f000011048000002c800006c18000000080000000 "expect_apdu_return 9000" \
	800f8100880300000000000000000000000000000000000000000000000000000000000000009e00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400000002037a0000000a076501000000013100020000ffdd6102321bc251e4a5190ad5b12b251069d9b401010000000000000000000000000000000000000000000000000764656661756c74 "expect_apdu_return 9c4f36db1d1258b08c88844f2f79b73361f5a9b3ff5fe89261cdce982756963525fbe358a31f56759eebdd9c137960ed24a14352d4c64e8792e2402b31360734ad9de6d7dd45aed49c78070b7718cf8469de0be71f7dafd2601900b3eecd350b9000"
expect_section_content nanos 'Operation (0)' 'Transfer ticket'
press_button right
expect_section_content nanos 'Fee' '0.01 tz'
press_button right
expect_section_content nanos 'Storage limit' '4'
press_button right
expect_section_content nanos 'Contents' 'UNPAIR'
press_button right
expect_section_content nanos 'Type' 'pair "1" 2'
press_button right
expect_section_content nanos 'Ticketer' 'tz1ixvCiPJYyMjsp2nKBVaq54f6AdbV8hCKa'
press_button right
expect_section_content nanos 'Amount' '1'
press_button right
expect_section_content nanos 'Destination' 'KT18amZmM5W7qDWVt2pH6uj7sCEd3kbzLrHT'
press_button right
expect_section_content nanos 'Entrypoint' 'default'
press_button right
expect_full_text 'Accept?'
press_button both
expect_async_apdus_sent
