module.exports = function(eleventyConfig) {
    eleventyConfig.addPassthroughCopy("./site/css/");
    eleventyConfig.addPassthroughCopy("./site/assets/")
    eleventyConfig.addPassthroughCopy("./site/js/")
    eleventyConfig.addPassthroughCopy("./site/CNAME")

    return {
        dir: {
            input: "site",
            output: "docs",
        },
    };
}