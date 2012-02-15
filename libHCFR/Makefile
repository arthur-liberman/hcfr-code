
all:
	rm ProfilesRepositoryIndexFileReader.cpp
	flex -t ProfilesRepositoryIndexFileReader.ll > ProfilesRepositoryIndexFileReader.cpp

	gcc -c Color.cpp matrix.cpp calibrationFile.cpp Exceptions.cpp ProfilesRepositoryIndexFileReader.cpp

	libtool -static -o libHCFR.a *.o

clean:
		rm *.o
