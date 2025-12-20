import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'GaussForge WASM Demo',
  description: 'Next.js demo for testing GaussForge WASM library',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}

