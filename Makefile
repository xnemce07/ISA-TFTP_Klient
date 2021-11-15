
Make:
	g++ src/mytftpclient.cpp src/tftp_lib.cpp src/options_parser.cpp -o mytftpclient
run:
	g++ src/mytftpclient.cpp src/tftp_lib.cpp src/options_parser.cpp -o mytftpclient
	./mytftpclient