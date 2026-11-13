-- Catalogue des vehicules supportes, gere par les super-admins.
-- Les utilisateurs normaux choisissent un vehicule DANS ce catalogue a
-- l'assignation d'un module : marque/modele/annee sont des listes deroulantes,
-- et le profil firmware a flasher en decoule. Seuls les admins editent le catalogue.

-- Qui est super-admin
create table public.admins (
  user_id uuid primary key references auth.users(id) on delete cascade
);

create or replace function public.is_admin() returns boolean
  language sql security definer stable set search_path = public as $$
  select exists (select 1 from public.admins a where a.user_id = auth.uid());
$$;

-- Le catalogue
create table public.vehicle_models (
  id                text primary key,              -- ex. 'seadoo-gtx-2023'
  type              text not null default 'jetski',-- jetski | boat | ...
  make              text not null,                 -- ex. 'Sea-Doo'
  model             text not null,                 -- ex. 'GTX'
  year              text not null,                 -- ex. '2023'
  firmware_profile  text not null,                 -- ex. 'profiles/gtx-2023.h'
  enabled           boolean not null default true  -- visible aux utilisateurs
);

alter table public.vehicle_models enable row level security;
revoke all on public.vehicle_models from authenticated, anon;
grant select on public.vehicle_models to authenticated;                 -- listes deroulantes
grant insert, update, delete on public.vehicle_models to authenticated; -- RLS limite aux admins

-- Tout le monde lit les modeles actifs ; seuls les admins voient le reste et editent.
create policy "catalogue lecture" on public.vehicle_models
  for select to authenticated using (enabled or public.is_admin());
create policy "catalogue admin" on public.vehicle_models
  for all to authenticated using (public.is_admin()) with check (public.is_admin());

-- Lien d'un module vers un vehicule du catalogue
alter table public.devices add column vehicle_model_id text references public.vehicle_models(id);
grant select (vehicle_model_id) on public.devices to authenticated;
grant update (vehicle_model_id) on public.devices to authenticated;

-- Catalogue de depart (les deux GTX). L'admin en ajoutera d'autres (bateaux, etc.).
insert into public.vehicle_models (id, type, make, model, year, firmware_profile) values
  ('seadoo-gtx-2023', 'jetski', 'Sea-Doo', 'GTX', '2023', 'profiles/gtx-2023.h'),
  ('seadoo-gtx-2026', 'jetski', 'Sea-Doo', 'GTX', '2026', 'profiles/gtx-2026.h');

-- Devenir super-admin : recupere ton user id (Authentication > Users dans Supabase),
-- puis dans l'editeur SQL :
--   insert into public.admins (user_id) values ('TON-UUID');
