# full input: 0000000000000000000000000000000000000000000000000000000000000000c900ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400000024000000083636366636663331000000083636366636663332000000083636366636663333
# signer: tz1dyX3B1CFYa2DfdFLyPtiJCfQRUgPVME6E
start_speculos "$seed"
expect_full_text 'Tezos Wallet' 'ready for' 'safe signing'
send_async_apdus \
	800f000011048000002c800006c18000000080000000 "expect_apdu_return 9000" \
	800f810064030000000000000000000000000000000000000000000000000000000000000000c900ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400000024000000083636366636663331000000083636366636663332000000083636366636663333 "expect_apdu_return e58af7dcac2c49b6599cbecb374d7082d94ee099bfd74ba2947b7dfd5a405bcc13feaadf66525b8f6eb200a604673e17808373ea8cdf8c381797fa27abc0ff21a6b116e460f95878923d0e030764cee67e4f3fd69dd23f05b02b7f412449c5099000"
expect_section_content nanosp 'Operation (0)' 'SR: send messages'
press_button right
expect_section_content nanosp 'Fee' '0.01 tz'
press_button right
expect_section_content nanosp 'Storage limit' '4'
press_button right
expect_section_content nanosp 'Message (0)' '666f6f31'
press_button right
expect_section_content nanosp 'Message (1)' '666f6f32'
press_button right
expect_section_content nanosp 'Message (2)' '666f6f33'
press_button right
expect_full_text 'Accept?' 'Press both buttons to accept.'
press_button both
expect_async_apdus_sent
