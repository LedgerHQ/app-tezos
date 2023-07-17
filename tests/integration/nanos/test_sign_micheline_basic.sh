start_speculos "$seed"
sleep 0.2
expect_full_text "Tezos Wallet" "ready for" "safe signing"
send_apdu 800f000011048000002c800006c18000000080000000
expect_apdu_return 9000
send_apdu 800f81002305020000001d0100000004434143410100000004504f504f0100000006424f5544494e
expect_full_text 'Expression' '{"CACA";"POPO";"BOUDIN"}'
press_button right
expect_full_text "Accept?"
press_button both
expect_apdu_return 84e475e38707140e725019e91f036e341fa4a2c8752b7828f37bbf91061b0e0ae0722bd72d15319474dff2207c137e85d57e742b7e5ccd1a995a610b8e055ad164e7606a37163b1a81e9003dc9e306afd46c4e645bbb190cf6c456459587ed049000
press_button right
press_button right
press_button right
press_button both
expect_exited
