-- Display-only artwork metadata; these codes are never jurisdiction records.
create table if not exists jurisdiction_flag_emblem(code text primary key,name text not null);
insert into jurisdiction_flag_emblem values('ZZ-SEA','Offshore')
on conflict(code) do update set name=excluded.name;
