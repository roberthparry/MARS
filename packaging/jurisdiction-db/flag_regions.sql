-- Independent display metadata. These identifiers do not change jurisdiction membership.
create table if not exists jurisdiction_flag_region(
    code text primary key,name text not null,parent_code text not null,artwork_code text not null);
delete from jurisdiction_flag_region;
insert into jurisdiction_flag_region values
('DE-NFR','North Frisia, Germany','DE','DE-NFR'),
('DE-OFR','East Frisia, Germany','DE','DE-OFR'),
('ES-CT','Catalonia, Spain','ES','ES-CT'),
('ES-IB','Balearic Islands, Spain','ES','ES-CT'),
('ES-NA','Navarre, Spain','ES','ES-PV'),
('ES-PV','Basque Country, Spain','ES','ES-PV'),
('ES-VC','Valencia, Spain','ES','ES-CT'),
('FR-BAS','Basque Country, France','FR','ES-PV'),
('FR-BRE','Brittany, France','FR','FR-BRE'),
('FR-CAT','Northern Catalonia, France','FR','ES-CT'),
('FR-COR','Corsica, France','FR','FR-COR'),
('GB-CON','Cornwall, England','GB-ENG','GB-CON'),
('IT-88','Sardinia, Italy','IT','IT-88'),
('NL-FR','West Frisia (Fryslân), Netherlands','NL','NL-FR');
