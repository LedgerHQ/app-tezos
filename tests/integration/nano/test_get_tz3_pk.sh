# Address expected: tz3UMNyvQeMj6mQSftW2aV2XaWd3afTAM1d5
start_speculos "$seed"
sleep 0.2
expected_home
send_apdu 8002000211048000002c800006c18000000080000000
expect_apdu_return 410497f4d381101d2908a13669313faec5dbf6693985584f96268ea2c25178199ddd1aad041e7564795eb4b9a4f379e8cdc0c8391f7b2880613771fff76e6a6b05cf9000
quit_app
