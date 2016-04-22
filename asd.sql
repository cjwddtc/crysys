drop table person;
drop table file;
CREATE TABLE person
(
id varchar NOT NULL,
ip varchar,
port int,
key blob,
PRIMARY KEY (id)
);
CREATE TABLE file
(
id varchar,
name varchar,
hash blob,
PRIMARY KEY (hash),
FOREIGN KEY (id) REFERENCES person(id)
);
