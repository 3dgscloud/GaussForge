import typescript from '@rollup/plugin-typescript';

// 外部依赖：不需要 Rollup 打包进来的文件
const webExternal = ['./gauss_forge.web.js'];
const nodeExternal = ['./gauss_forge.node.js', 'path', 'fs', 'module', 'crypto', 'url'];

export default [
  // 1. Web ESM Build
  {
    input: 'index.web.ts', // 对应你的 index.web.ts
    output: {
      dir: 'dist',
      entryFileNames: 'index.web.js', // 对应 package.json 的 browser 导出
      format: 'es',
      sourcemap: true,
    },
    plugins: [
      typescript({ tsconfig: './tsconfig.json', declaration: true, declarationDir: 'dist' }),
    ],
    external: webExternal,
  },
  // 2. Node.js Build (支持 ESM 和 CJS)
  {
    input: 'index.node.ts', // 对应你的 index.node.ts
    output: [
      {
        dir: 'dist',
        entryFileNames: 'index.node.js', // 对应 package.json 的 node/require 导出
        format: 'es', // 现代 Node 建议使用 ES 格式
        sourcemap: true,
      }
    ],
    plugins: [
      typescript({ tsconfig: './tsconfig.json', declaration: false }),
    ],
    external: nodeExternal,
  },
];