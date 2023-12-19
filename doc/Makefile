MANS = yass_cli.1 yass_server.1

%.1: %.md
	ronn $< -o .

all: $(MANS)

clean:
	rm -vf *.1
