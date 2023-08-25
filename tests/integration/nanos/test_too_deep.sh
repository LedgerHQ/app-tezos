# full input: 02000000f702000000f202000000ed02000000e802000000e302000000de02000000d902000000d402000000cf02000000ca02000000c502000000c002000000bb02000000b602000000b102000000ac02000000a702000000a2020000009d02000000980200000093020000008e02000000890200000084020000007f020000007a02000000750200000070020000006b02000000660200000061020000005c02000000570200000052020000004d02000000480200000043020000003e02000000390200000034020000002f020000002a02000000250200000020020000001b02000000160200000011020000000c02000000070200000002002a
# full output: {{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{42}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
# signer: tz1dyX3B1CFYa2DfdFLyPtiJCfQRUgPVME6E
start_speculos "$seed"
expect_full_text 'ready for' 'safe signing'
send_apdu 800f000011048000002c800006c18000000080000000
expect_apdu_return 9000
send_apdu 800f0100eb0502000000f702000000f202000000ed02000000e802000000e302000000de02000000d902000000d402000000cf02000000ca02000000c502000000c002000000bb02000000b602000000b102000000ac02000000a702000000a2020000009d02000000980200000093020000008e02000000890200000084020000007f020000007a02000000750200000070020000006b02000000660200000061020000005c02000000570200000052020000004d02000000480200000043020000003e02000000390200000034020000002f020000002a02000000250200000020020000001b020000001602000000
expect_full_text 'Expression' '{{{{{{{{{{{{{{{{{{{'
press_button right
expect_full_text 'Expression' '{{{{{{{{{{{{{{{{{{{'
press_button right
expect_full_text 'Parsing error'
press_button both
expect_apdu_return 9405
expect_full_text 'ready for' 'safe signing'
press_button right
expect_full_text 'Settings'
press_button right
expect_full_text 'Quit?'
press_button both
expect_exited
