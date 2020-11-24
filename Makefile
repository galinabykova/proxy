all:
	 g++ -g Tuda.cpp Cache.cpp Suda.cpp HTTP.cpp main.cpp -lsocket -lnsl -o proxy
