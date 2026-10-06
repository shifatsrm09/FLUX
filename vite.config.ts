import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,
    host: true, // Listen on all addresses (0.0.0.0) so mobile devices on the same Wi-Fi / LAN can access http://<PC_IP>:3000
  },
});
