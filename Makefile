all:
	 g++ -g Tuda.cpp Cache.cpp Suda.cpp HTTP.cpp CritException.cpp main.cpp -lsocket -lnsl -o proxy
