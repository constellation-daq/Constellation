/* Animation on front-page */
document.addEventListener("DOMContentLoaded", () => {
    if (!document.querySelector(".typewrite"))
        return;

    const el = document.querySelector(".typewrite");
    if (!el)
        return;

    class TypeWriter {

        constructor(el, words, period) {
            this.el = el;
            this.words = words;
            this.period = parseInt(period, 10) || 2000;
            this.txt = '';
            this.wordIndex = 0;
            this.isDeleting = false;
            this.tick();
        }

        tick() {
            const i = this.wordIndex % this.words.length;
            const fullTxt = this.words[i];

            this.txt = this.isDeleting
                           ? fullTxt.substring(0, this.txt.length - 1)
                           : fullTxt.substring(0, this.txt.length + 1);

            this.el.innerHTML = this.txt;

            let delta = this.isDeleting ? 50 : 90;

            if (!this.isDeleting && this.txt === fullTxt) {
                delta = this.period;
                this.isDeleting = true;
            } else if (this.isDeleting && this.txt === '') {
                this.isDeleting = false;
                this.wordIndex++;
                delta = 400;
            }

            setTimeout(() => this.tick(), delta);
        }
    }

    new TypeWriter(el, JSON.parse(el.getAttribute("data-type")),
                   el.getAttribute("data-period"));
});
