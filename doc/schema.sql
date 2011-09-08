CREATE TABLE cache
(
	id bigint auto_increment not null,
	hash char(32) unique not null,
	url longtext not null,
	decision enum('PORN', 'BENIGN', 'BIKINI', 'UNDEFINED') not null,
	ctype mediumtext not null,
	status enum('FETCHING', 'PROCESSING', 'FETCHING_MORE', 'CLASSIFYING', 'DONE', 'FAILURE') not null,
	primary key(id),
	unique index (url(1000)) using hash,
	unique index (hash) using hash
) engine=myisam;
