const board = document.querySelector('#board');
const installer = document.querySelector('#installer');
const button = document.querySelector('#install-button');
const error = document.querySelector('#load-error');
let manifest;

function selectBoard() {
  installer.setAttribute('manifest', `manifest-${board.value}.json`);
  if (manifest) {
    document.querySelector('#download').href =
      `firmware/ChainOSCPad-MIDI-${manifest.version}-${board.value}-factory.bin`;
  }
}

board.addEventListener('change', selectBoard);

try {
  const response = await fetch('manifest.json', { cache: 'no-cache' });
  if (!response.ok) throw new Error(`マニフェストの取得に失敗しました (${response.status})`);
  manifest = await response.json();
  if (!/^\d+\.\d+\.\d+$/.test(manifest.version) || manifest.builds?.length !== 4) {
    throw new Error('公開ファームウェアの情報が不正です');
  }
  document.querySelector('#version').textContent = `Release v${manifest.version}`;
  selectBoard();
  await Promise.race([
    customElements.whenDefined('esp-web-install-button'),
    new Promise((_, reject) => setTimeout(() => reject(new Error('書き込みUIを読み込めません。ネットワーク接続を確認して再読み込みしてください。')), 15000)),
  ]);
  button.disabled = false;
} catch (cause) {
  document.querySelector('#version').textContent = 'インストーラーを読み込めません';
  error.textContent = cause.message;
  error.hidden = false;
}
