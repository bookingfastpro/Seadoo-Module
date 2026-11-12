// Copier en "config.js" et remplir (Supabase > Project Settings > API).
// La cle "anon" est publique par nature : la securite vient de la connexion
// par e-mail/mot de passe et des regles RLS.
window.SEADOO_CONFIG = {
  supabaseUrl: "https://VOTRE-PROJET.supabase.co",
  supabaseAnonKey: "VOTRE-CLE-ANON",

  // MQTT temps reel (optionnel). Laisser mqttUrl vide pour rester en mode
  // rafraichissement Supabase toutes les 15 s. Avec, la carte bouge en direct.
  // URL "WebSocket securise" de votre broker (voir docs/06-mqtt.md).
  mqttUrl: "",                 // ex. "wss://xxxx.s1.eu.hivemq.cloud:8884/mqtt"
  mqttUser: "dashboard",
  mqttPass: "",
};
